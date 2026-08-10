/*
 * ============================================================================
 * DS REST API SERVER v1.0
 * ============================================================================
 *
 * Read-only HTTP/JSON REST API for DS Battery Management, Torque Enhancement,
 * and Regenerative Braking system.
 *
 * FEATURES:
 *   - Observability-only (GET endpoints only; no control surfaces)
 *   - Lightweight POSIX sockets-based HTTP server
 *   - Runs in dedicated thread; does not block real-time control loops
 *   - Thread-safe with mutex-protected shared data access
 *   - Binds to 0.0.0.0:8080 for LAN-wide access
 *
 * ENDPOINTS:
 *   GET /health                - Server health check
 *   GET /api/battery           - Battery state (Ψ physical, Φ informational)
 *   GET /api/torque            - Torque limits and limiting factors
 *   GET /api/regen             - Regen limits and charge-acceptance constraints
 *   GET /api/limits            - Combined limits (battery, torque, regen)
 *   GET /api/diagnostics       - Pack diagnostics (weak cells, imbalance, thermal)
 *   GET /api/energy_cycle      - Energy cycle metrics
 *
 * INTEGRATION:
 *   ds::api::RestApiServer server;
 *   server.start();  // Starts in background thread
 *   // ... your control loop ...
 *   server.update_state(enhanced_state, torque_result, regen_result, diag);
 *   // ... continue control loop ...
 *   server.stop();   // Clean shutdown
 *
 * AUTHORS: Don Michael Feeney Jr.
 * DATE: February 2026
 * LICENSE: Copyright (c) Don Michael Feeney Jr. Licensed under the MIT License.
 * VERSION: 1.0.0
 *
 * ============================================================================
 */

#ifndef DS_REST_API_HPP
#define DS_REST_API_HPP

#include "ds_battery_enhancement.hpp"
#include "ds_bms_middleware_v2.hpp"
#include "torque_enhancement.hpp"
#include "ds_regen_braking_manager_v1.hpp"

#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <map>
#include <functional>

// POSIX socket headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace ds {
namespace api {

// ============================================================================
// VERSION INFORMATION
// ============================================================================

constexpr int API_VERSION_MAJOR = 1;
constexpr int API_VERSION_MINOR = 0;
constexpr int API_VERSION_PATCH = 0;

inline std::string get_api_version() {
    return std::to_string(API_VERSION_MAJOR) + "." +
           std::to_string(API_VERSION_MINOR) + "." +
           std::to_string(API_VERSION_PATCH);
}

// ============================================================================
// JSON UTILITIES (Simple header-only JSON serialization)
// ============================================================================

class JsonBuilder {
private:
    std::ostringstream ss_;
    bool first_field_ = true;

public:
    JsonBuilder() { ss_ << "{"; }

    void add_string(const std::string& key, const std::string& value) {
        if (!first_field_) ss_ << ",";
        ss_ << "\"" << key << "\":\"" << escape_json(value) << "\"";
        first_field_ = false;
    }

    void add_number(const std::string& key, double value) {
        if (!first_field_) ss_ << ",";
        ss_ << "\"" << key << "\":" << value;
        first_field_ = false;
    }

    void add_bool(const std::string& key, bool value) {
        if (!first_field_) ss_ << ",";
        ss_ << "\"" << key << "\":" << (value ? "true" : "false");
        first_field_ = false;
    }

    void add_array_start(const std::string& key) {
        if (!first_field_) ss_ << ",";
        ss_ << "\"" << key << "\":[";
        first_field_ = true;  // Reset for array items
    }

    void add_array_number(double value) {
        if (!first_field_) ss_ << ",";
        ss_ << value;
        first_field_ = false;
    }

    void add_array_end() {
        ss_ << "]";
        first_field_ = false;
    }

    void add_object_start(const std::string& key) {
        if (!first_field_) ss_ << ",";
        ss_ << "\"" << key << "\":{";
        first_field_ = true;  // Reset for object fields
    }

    void add_object_end() {
        ss_ << "}";
        first_field_ = false;
    }

    std::string build() {
        ss_ << "}";
        return ss_.str();
    }

private:
    std::string escape_json(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        result += buf;
                    } else {
                        result += c;
                    }
                    break;
            }
        }
        return result;
    }
};

// ============================================================================
// THREAD-SAFE STATE SNAPSHOT
// ============================================================================

struct ApiStateSnapshot {
    // Timestamp
    double timestamp_ms;
    
    // Battery state
    ds::EnhancedState battery_state;
    
    // Torque result
    ds::drive::TorqueResult torque_result;
    
    // Regen result
    ds::drive::RegenResult regen_result;
    
    // Diagnostics
    ds_plugin::DiagnosticReport diagnostics;
    
    // Energy cycle metrics
    struct EnergyCycle {
        double drive_energy_kwh = 0.0;
        double regen_energy_kwh = 0.0;
        double loss_energy_kwh = 0.0;
        double delta_info_energy_j = 0.0;
        double total_cycles = 0.0;
        double efficiency_percent = 0.0;
    } energy_cycle;
    
    // Safety status
    struct SafetyStatus {
        bool thermal_ok = true;
        bool soc_ok = true;
        bool cell_protection_ok = true;
        bool voltage_ok = true;
        bool current_ok = true;
    } safety;
    
    ApiStateSnapshot() : timestamp_ms(0) {}
};

// ============================================================================
// HTTP REQUEST/RESPONSE
// ============================================================================

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
};

struct HttpResponse {
    int status_code;
    std::string status_message;
    std::string content_type;
    std::string body;
    
    HttpResponse() : status_code(200), status_message("OK"), 
                     content_type("application/json") {}
    
    std::string to_string() const {
        std::ostringstream ss;
        ss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
        ss << "Content-Type: " << content_type << "\r\n";
        ss << "Content-Length: " << body.length() << "\r\n";
        ss << "Connection: close\r\n";
        ss << "Access-Control-Allow-Origin: *\r\n";
        ss << "X-Content-Type-Options: nosniff\r\n";
        ss << "\r\n";
        ss << body;
        return ss.str();
    }
};

// ============================================================================
// REST API SERVER
// ============================================================================

class RestApiServer {
private:
    // Server configuration
    std::string bind_address_ = "0.0.0.0";
    int bind_port_ = 8080;
    
    // Server state
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    int server_socket_ = -1;
    
    // Shared state (mutex-protected)
    mutable std::mutex state_mutex_;
    ApiStateSnapshot current_state_;
    
    // Request statistics
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> successful_requests_{0};
    std::atomic<uint64_t> failed_requests_{0};
    
public:
    RestApiServer(const std::string& address = "0.0.0.0", int port = 8080)
        : bind_address_(address), bind_port_(port) {}
    
    ~RestApiServer() {
        stop();
    }
    
    RestApiServer(const RestApiServer&) = delete;
    RestApiServer& operator=(const RestApiServer&) = delete;
    RestApiServer(RestApiServer&&) = delete;
    RestApiServer& operator=(RestApiServer&&) = delete;
    
    // Start the API server in a background thread
    bool start() {
        if (running_.load()) {
            return false;  // Already running
        }
        
        // Create socket
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0) {
            return false;
        }
        
        // Set socket options
        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Bind to address
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(bind_address_.c_str());
        server_addr.sin_port = htons(bind_port_);
        
        if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(server_socket_);
            server_socket_ = -1;
            return false;
        }
        
        // Listen for connections
        if (listen(server_socket_, 10) < 0) {
            close(server_socket_);
            server_socket_ = -1;
            return false;
        }
        
        // Start server thread
        running_.store(true);
        server_thread_ = std::thread(&RestApiServer::server_loop, this);
        
        return true;
    }
    
    // Stop the API server
    void stop() {
        if (!running_.load()) {
            return;
        }
        
        running_.store(false);
        
        // Close server socket to unblock accept()
        if (server_socket_ >= 0) {
            shutdown(server_socket_, SHUT_RDWR);
            close(server_socket_);
            server_socket_ = -1;
        }
        
        // Wait for thread to finish
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
    // Update shared state (called from control loop)
    void update_state(
        const ds::EnhancedState& battery_state,
        const ds::drive::TorqueResult& torque_result,
        const ds::drive::RegenResult& regen_result,
        const ds_plugin::DiagnosticReport& diagnostics)
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        // Update timestamp
        current_state_.timestamp_ms = get_timestamp_ms();
        
        // Update states
        current_state_.battery_state = battery_state;
        current_state_.torque_result = torque_result;
        current_state_.regen_result = regen_result;
        current_state_.diagnostics = diagnostics;
        
        // Update safety status
        update_safety_status(current_state_.safety);
    }
    
    // Update energy cycle metrics
    void update_energy_cycle(double drive_kwh, double regen_kwh, double loss_kwh, 
                             double delta_info_j, double cycles, double efficiency_pct)
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        current_state_.energy_cycle.drive_energy_kwh = drive_kwh;
        current_state_.energy_cycle.regen_energy_kwh = regen_kwh;
        current_state_.energy_cycle.loss_energy_kwh = loss_kwh;
        current_state_.energy_cycle.delta_info_energy_j = delta_info_j;
        current_state_.energy_cycle.total_cycles = cycles;
        current_state_.energy_cycle.efficiency_percent = efficiency_pct;
    }
    
    // Get request statistics
    struct Stats {
        uint64_t total_requests;
        uint64_t successful_requests;
        uint64_t failed_requests;
    };
    
    Stats get_stats() const {
        return {total_requests_.load(), successful_requests_.load(), failed_requests_.load()};
    }
    
private:
    // Server loop (runs in background thread)
    void server_loop() {
        while (running_.load()) {
            // Accept connection
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket < 0) {
                if (running_.load()) {
                    // Only log error if we're still supposed to be running
                    continue;
                }
                break;
            }
            
            // Set timeout for client socket
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            // Handle request
            handle_client(client_socket);
            
            // Close connection
            close(client_socket);
        }
    }
    
    // Handle a single client connection
    void handle_client(int client_socket) {
        total_requests_++;
        
        // Read request
        char buffer[4096];
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            failed_requests_++;
            return;
        }
        
        buffer[bytes_read] = '\0';
        
        // Parse request
        HttpRequest request = parse_request(buffer);
        
        // Route request
        HttpResponse response = route_request(request);
        
        // Send response
        std::string response_str = response.to_string();
        send(client_socket, response_str.c_str(), response_str.length(), MSG_NOSIGNAL);
        
        if (response.status_code == 200) {
            successful_requests_++;
        } else {
            failed_requests_++;
        }
    }
    
    // Parse HTTP request
    HttpRequest parse_request(const char* buffer) {
        HttpRequest request;
        std::istringstream ss(buffer);
        
        // Parse request line
        ss >> request.method >> request.path >> request.version;
        
        return request;
    }
    
    // Route request to appropriate handler
    HttpResponse route_request(const HttpRequest& request) {
        // Only allow GET requests
        if (request.method != "GET") {
            return create_error_response(405, "Method Not Allowed");
        }
        
        // Path normalization: reject suspicious paths
        const std::string& path = request.path;
        if (path.find("..") != std::string::npos ||
            path.find("//") != std::string::npos ||
            path.find('%') != std::string::npos) {
            return create_error_response(400, "Bad Request");
        }
        
        // Route to handlers
        if (request.path == "/health") {
            return handle_health();
        } else if (request.path == "/api/battery") {
            return handle_battery();
        } else if (request.path == "/api/torque") {
            return handle_torque();
        } else if (request.path == "/api/regen") {
            return handle_regen();
        } else if (request.path == "/api/limits") {
            return handle_limits();
        } else if (request.path == "/api/diagnostics") {
            return handle_diagnostics();
        } else if (request.path == "/api/energy_cycle") {
            return handle_energy_cycle();
        } else {
            return create_error_response(404, "Not Found");
        }
    }
    
    // GET /health
    HttpResponse handle_health() {
        JsonBuilder json;
        json.add_string("status", "ok");
        json.add_string("version", get_api_version());
        json.add_number("timestamp_ms", get_timestamp_ms());
        
        auto stats = get_stats();
        json.add_number("total_requests", static_cast<double>(stats.total_requests));
        json.add_number("successful_requests", static_cast<double>(stats.successful_requests));
        json.add_number("failed_requests", static_cast<double>(stats.failed_requests));
        
        HttpResponse response;
        response.body = json.build();
        return response;
    }
    
    // GET /api/battery
    HttpResponse handle_battery() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        JsonBuilder json;
        json.add_number("timestamp_ms", current_state_.timestamp_ms);
        
        // Physical state (Ψ)
        json.add_object_start("physical_state");
        json.add_number("voltage_v", current_state_.battery_state.state.voltage);
        json.add_number("current_a", current_state_.battery_state.state.current);
        json.add_number("temperature_c", current_state_.battery_state.state.temperature);
        json.add_number("state_of_charge", current_state_.battery_state.state.state_of_charge);
        json.add_object_end();
        
        // Informational state (Φ)
        json.add_object_start("informational_state");
        json.add_number("entropy", current_state_.battery_state.state.entropy);
        json.add_number("cycle_count", current_state_.battery_state.state.cycle_count);
        json.add_number("degradation", current_state_.battery_state.state.degradation);
        json.add_number("phi_magnitude", current_state_.battery_state.state.phi_magnitude);
        json.add_object_end();
        
        // Health prediction
        json.add_object_start("health");
        json.add_number("remaining_capacity_percent", 
            current_state_.battery_state.health.remaining_capacity_percent);
        json.add_number("cycles_to_80_percent", 
            current_state_.battery_state.health.cycles_to_80_percent);
        json.add_number("estimated_eol_cycles", 
            current_state_.battery_state.health.estimated_eol_cycles);
        json.add_number("confidence", current_state_.battery_state.health.confidence);
        json.add_bool("warning_triggered", current_state_.battery_state.health.warning_triggered);
        json.add_object_end();
        
        // Energy tracking
        json.add_object_start("energy");
        json.add_number("energy_psi_j", current_state_.battery_state.state.energy_psi);
        json.add_number("energy_phi_j", current_state_.battery_state.state.energy_phi);
        json.add_number("energy_metric_j", current_state_.battery_state.state.energy_metric);
        json.add_number("energy_total_j", current_state_.battery_state.state.energy_total);
        json.add_object_end();
        
        HttpResponse response;
        response.body = json.build();
        return response;
    }
    
    // GET /api/torque
    HttpResponse handle_torque() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        JsonBuilder json;
        json.add_number("timestamp_ms", current_state_.timestamp_ms);
        
        // Torque limits
        json.add_number("max_drive_torque_nm", current_state_.torque_result.max_drive_torque_nm);
        json.add_number("max_regen_torque_nm", current_state_.torque_result.max_regen_torque_nm);
        json.add_number("max_power_kw", current_state_.torque_result.max_power_kw);
        
        // Scaling factors
        json.add_object_start("scaling_factors");
        json.add_number("base_motor", current_state_.torque_result.base_motor_scaling);
        json.add_number("ds", current_state_.torque_result.ds_scaling);
        json.add_number("thermal", current_state_.torque_result.thermal_scaling);
        json.add_number("soc", current_state_.torque_result.soc_scaling);
        json.add_number("health", current_state_.torque_result.health_scaling);
        json.add_number("cell_balancing", current_state_.torque_result.cell_balancing_scaling);
        json.add_number("overall", current_state_.torque_result.overall_scaling);
        json.add_object_end();
        
        // Active derating
        json.add_object_start("derate_active");
        json.add_bool("health", current_state_.torque_result.health_derate_active);
        json.add_bool("thermal", current_state_.torque_result.thermal_derate_active);
        json.add_bool("entropy", current_state_.torque_result.entropy_derate_active);
        json.add_bool("metric", current_state_.torque_result.metric_derate_active);
        json.add_bool("soc", current_state_.torque_result.soc_derate_active);
        json.add_bool("weak_cell", current_state_.torque_result.weak_cell_derate_active);
        json.add_object_end();
        
        // Limiting factor
        json.add_string("limiting_factor", current_state_.torque_result.limiting_factor);
        json.add_number("confidence_level", current_state_.torque_result.confidence_level);
        
        HttpResponse response;
        response.body = json.build();
        return response;
    }
    
    // GET /api/regen
    HttpResponse handle_regen() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        JsonBuilder json;
        json.add_number("timestamp_ms", current_state_.timestamp_ms);
        
        // Regen limits
        json.add_number("max_regen_torque_nm", current_state_.regen_result.max_regen_torque_nm);
        json.add_number("regen_fraction", current_state_.regen_result.regen_fraction);
        json.add_string("limiting_factor", current_state_.regen_result.limiting_factor);
        
        // Derate factors
        json.add_object_start("derate_factors");
        json.add_number("f_speed", current_state_.regen_result.diag.f_speed);
        json.add_number("f_soc", current_state_.regen_result.diag.f_soc);
        json.add_number("f_voltage", current_state_.regen_result.diag.f_voltage);
        json.add_number("f_temp", current_state_.regen_result.diag.f_temp);
        json.add_number("f_cell", current_state_.regen_result.diag.f_cell);
        json.add_number("f_ds", current_state_.regen_result.diag.f_ds);
        json.add_number("f_stability", current_state_.regen_result.diag.f_stability);
        json.add_object_end();
        
        // Events
        json.add_object_start("events");
        json.add_number("abs_events", current_state_.regen_result.diag.abs_events);
        json.add_number("slip_events", current_state_.regen_result.diag.slip_events);
        json.add_number("safety_blocks", current_state_.regen_result.diag.safety_blocks);
        json.add_object_end();
        
        HttpResponse response;
        response.body = json.build();
        return response;
    }
    
    // GET /api/limits
    HttpResponse handle_limits() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        JsonBuilder json;
        json.add_number("timestamp_ms", current_state_.timestamp_ms);
        
        // Battery limits
        json.add_object_start("battery");
        json.add_number("soc", current_state_.battery_state.state.state_of_charge);
        json.add_number("max_charge_current_a", 
            current_state_.battery_state.charging.recommended_current_limit);
        json.add_number("max_discharge_current_a", 
            current_state_.battery_state.charging.max_safe_current);
        json.add_object_end();
        
        // Torque limits
        json.add_object_start("torque");
        json.add_number("max_drive_torque_nm", current_state_.torque_result.max_drive_torque_nm);
        json.add_number("max_regen_torque_nm", current_state_.torque_result.max_regen_torque_nm);
        json.add_number("max_power_kw", current_state_.torque_result.max_power_kw);
        json.add_object_end();
        
        // Regen limits
        json.add_object_start("regen");
        json.add_number("max_regen_torque_nm", current_state_.regen_result.max_regen_torque_nm);
        json.add_number("regen_fraction", current_state_.regen_result.regen_fraction);
        json.add_object_end();
        
        HttpResponse response;
        response.body = json.build();
        return response;
    }
    
    // GET /api/diagnostics
    HttpResponse handle_diagnostics() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        JsonBuilder json;
        json.add_number("timestamp_ms", current_state_.timestamp_ms);
        
        // Pack diagnostics
        json.add_number("pack_health_percent", current_state_.diagnostics.pack_health_percent);
        json.add_number("estimated_remaining_cycles", 
            current_state_.diagnostics.estimated_remaining_cycles);
        json.add_number("degradation_rate_per_cycle", 
            current_state_.diagnostics.degradation_rate_per_cycle);
        
        // Cell-level diagnostics
        json.add_array_start("weak_cell_ids");
        for (int id : current_state_.diagnostics.weak_cell_ids) {
            json.add_array_number(static_cast<double>(id));
        }
        json.add_array_end();
        
        json.add_number("voltage_imbalance_mv", current_state_.diagnostics.voltage_imbalance_mv);
        
        // Calculate temperature spread from min/max
        double temp_spread = current_state_.diagnostics.max_cell_temp_c - 
                            current_state_.diagnostics.min_cell_temp_c;
        json.add_number("temperature_spread_celsius", temp_spread);
        
        // Warnings
        json.add_object_start("warnings");
        json.add_bool("degradation", current_state_.diagnostics.degradation_warning);
        json.add_bool("thermal", current_state_.diagnostics.thermal_warning);
        json.add_bool("imbalance", current_state_.diagnostics.balancing_required);
        json.add_bool("weak_cell", current_state_.diagnostics.weak_cell_warning);
        json.add_object_end();
        
        // DS metrics
        json.add_object_start("ds_metrics");
        json.add_number("metric_trace", current_state_.diagnostics.ds_metric_trace);
        json.add_number("phi_magnitude", current_state_.diagnostics.ds_phi_magnitude);
        json.add_number("entropy_level", current_state_.diagnostics.ds_entropy);
        json.add_number("confidence", current_state_.diagnostics.ds_confidence);
        json.add_object_end();
        
        // Additional fields
        json.add_number("total_cells", current_state_.diagnostics.total_cells);
        json.add_number("weak_cell_count", current_state_.diagnostics.weak_cell_count);
        
        HttpResponse response;
        response.body = json.build();
        return response;
    }
    
    // GET /api/energy_cycle
    HttpResponse handle_energy_cycle() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        JsonBuilder json;
        json.add_number("timestamp_ms", current_state_.timestamp_ms);
        
        // Energy metrics
        json.add_number("drive_energy_kwh", current_state_.energy_cycle.drive_energy_kwh);
        json.add_number("regen_energy_kwh", current_state_.energy_cycle.regen_energy_kwh);
        json.add_number("loss_energy_kwh", current_state_.energy_cycle.loss_energy_kwh);
        json.add_number("delta_info_energy_j", current_state_.energy_cycle.delta_info_energy_j);
        json.add_number("total_cycles", current_state_.energy_cycle.total_cycles);
        json.add_number("efficiency_percent", current_state_.energy_cycle.efficiency_percent);
        
        // Safety status
        json.add_object_start("safety");
        json.add_bool("thermal_ok", current_state_.safety.thermal_ok);
        json.add_bool("soc_ok", current_state_.safety.soc_ok);
        json.add_bool("cell_protection_ok", current_state_.safety.cell_protection_ok);
        json.add_bool("voltage_ok", current_state_.safety.voltage_ok);
        json.add_bool("current_ok", current_state_.safety.current_ok);
        json.add_object_end();
        
        HttpResponse response;
        response.body = json.build();
        return response;
    }
    
    // Create error response
    HttpResponse create_error_response(int status_code, const std::string& message) {
        JsonBuilder json;
        json.add_number("error_code", static_cast<double>(status_code));
        json.add_string("error_message", message);
        
        HttpResponse response;
        response.status_code = status_code;
        response.status_message = message;
        response.body = json.build();
        return response;
    }
    
    // Get current timestamp in milliseconds
    static double get_timestamp_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
    }
    
    // Update safety status based on current state
    void update_safety_status(ApiStateSnapshot::SafetyStatus& safety) {
        const auto& state = current_state_.battery_state.state;
        
        // Thermal check
        safety.thermal_ok = (state.temperature >= -20.0 && state.temperature <= 60.0);
        
        // SOC check
        safety.soc_ok = (state.state_of_charge >= 0.05 && state.state_of_charge <= 0.98);
        
        // Voltage check (basic sanity)
        safety.voltage_ok = (state.voltage > 0.0 && state.voltage < 500.0);
        
        // Current check (basic sanity)
        safety.current_ok = (std::abs(state.current) < 600.0);
        
        // Cell protection (based on diagnostics)
        safety.cell_protection_ok = !current_state_.diagnostics.weak_cell_warning &&
                                    !current_state_.diagnostics.balancing_required;
    }
};

} // namespace api
} // namespace ds

#endif // DS_REST_API_HPP
