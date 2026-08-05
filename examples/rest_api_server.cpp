/*
 * ============================================================================
 * DS REST API SERVER - EXAMPLE APPLICATION
 * ============================================================================
 *
 * This example demonstrates how to integrate the DS REST API server with
 * your battery management, torque enhancement, and regenerative braking
 * control loops.
 *
 * The API server runs in a background thread and provides read-only access
 * to the system state without interfering with real-time control.
 *
 * Compile:
 *   g++ -std=c++17 -O3 -I../include examples/rest_api_server.cpp -o rest_api_server -lpthread
 *
 * Run:
 *   ./rest_api_server
 *
 * Test endpoints:
 *   curl http://localhost:8080/health
 *   curl http://localhost:8080/api/battery
 *   curl http://localhost:8080/api/torque
 *   curl http://localhost:8080/api/regen
 *   curl http://localhost:8080/api/limits
 *   curl http://localhost:8080/api/diagnostics
 *   curl http://localhost:8080/api/energy_cycle
 *
 * ============================================================================
 */

#include "ds_battery_enhancement.hpp"
#include "ds_bms_middleware_v2.hpp"
#include "torque_enhancement.hpp"
#include "ds_regen_braking_manager_v1.hpp"
#include "ds_rest_api.hpp"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal, stopping server...\n";
        g_running.store(false);
    }
}

// ============================================================================
// SIMULATED VEHICLE STATE
// ============================================================================

struct VehicleState {
    double time_s = 0.0;
    double speed_kph = 0.0;
    double motor_rpm = 0.0;
    double accelerator_position = 0.0;  // 0.0 to 1.0
    double brake_position = 0.0;        // 0.0 to 1.0
    bool abs_active = false;
    bool esc_active = false;
};

// Simulate a driving cycle
VehicleState simulate_vehicle(double time) {
    VehicleState state;
    state.time_s = time;
    
    // Create a repeating drive cycle: accelerate, cruise, brake, stop
    double cycle_time = std::fmod(time, 60.0);  // 60 second cycle
    
    if (cycle_time < 15.0) {
        // Acceleration phase (0-15s)
        double t = cycle_time / 15.0;
        state.speed_kph = 80.0 * t;
        state.accelerator_position = 0.7 * (1.0 - t);
        state.brake_position = 0.0;
    } else if (cycle_time < 35.0) {
        // Cruise phase (15-35s)
        state.speed_kph = 80.0;
        state.accelerator_position = 0.3;
        state.brake_position = 0.0;
    } else if (cycle_time < 50.0) {
        // Braking phase (35-50s)
        double t = (cycle_time - 35.0) / 15.0;
        state.speed_kph = 80.0 * (1.0 - t);
        state.accelerator_position = 0.0;
        state.brake_position = 0.6 * t;
    } else {
        // Stopped phase (50-60s)
        state.speed_kph = 0.0;
        state.accelerator_position = 0.0;
        state.brake_position = 0.0;
    }
    
    // Convert speed to motor RPM (simplified)
    state.motor_rpm = state.speed_kph * 100.0;  // ~10,000 RPM at 100 kph
    
    return state;
}

// Simulate battery sensors
struct BatterySensors {
    double voltage;
    double current;
    double temperature;
    double soc;
};

BatterySensors simulate_battery(double time, const VehicleState& vehicle) {
    BatterySensors sensors;
    
    // Base values
    double base_soc = 0.7 + 0.2 * std::sin(time * 0.01);  // Slowly varying SOC
    sensors.soc = std::max(0.1, std::min(0.95, base_soc));
    
    // Voltage depends on SOC and current
    sensors.voltage = 350.0 + 50.0 * sensors.soc;
    
    // Current depends on accelerator/brake
    if (vehicle.accelerator_position > 0.0) {
        sensors.current = -vehicle.accelerator_position * 200.0;  // Discharge
    } else if (vehicle.brake_position > 0.0) {
        sensors.current = vehicle.brake_position * 100.0;  // Regen charge
    } else {
        sensors.current = 0.0;
    }
    
    // Temperature varies with load (keep within safe range -20 to 60°C)
    double heat_load = std::abs(sensors.current * sensors.voltage) / 1000.0;  // kW
    double base_temp = 25.0 + heat_load * 0.5 + 5.0 * std::sin(time * 0.1);
    sensors.temperature = std::max(-10.0, std::min(55.0, base_temp));
    
    return sensors;
}

// ============================================================================
// MAIN CONTROL LOOP
// ============================================================================

int main() {
    std::cout << "==========================================================\n";
    std::cout << "DS REST API Server - Example Application\n";
    std::cout << "==========================================================\n\n";
    
    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Initialize DS middleware
    std::cout << "Initializing DS BMS Middleware...\n";
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0);  // 75Ah, 400V battery
    
    // Initialize torque manager
    std::cout << "Initializing DS Torque Manager...\n";
    ds::drive::TorqueConfig torque_config;
    ds::drive::DSTorqueManager torque_mgr;
    torque_mgr.init(torque_config);
    
    // Initialize regen manager
    std::cout << "Initializing DS Regen Manager...\n";
    ds::drive::RegenConfig regen_config;
    ds::drive::DSRegenBrakingManager regen_mgr(regen_config);
    
    // Initialize and start REST API server
    std::cout << "Starting REST API server on 0.0.0.0:8080...\n";
    ds::api::RestApiServer api_server("0.0.0.0", 8080);
    
    if (!api_server.start()) {
        std::cerr << "ERROR: Failed to start API server!\n";
        std::cerr << "Make sure port 8080 is available.\n";
        return 1;
    }
    
    std::cout << "\n==========================================================\n";
    std::cout << "Server running! Test with:\n";
    std::cout << "  curl http://localhost:8080/health\n";
    std::cout << "  curl http://localhost:8080/api/battery\n";
    std::cout << "  curl http://localhost:8080/api/torque\n";
    std::cout << "  curl http://localhost:8080/api/regen\n";
    std::cout << "  curl http://localhost:8080/api/limits\n";
    std::cout << "  curl http://localhost:8080/api/diagnostics\n";
    std::cout << "  curl http://localhost:8080/api/energy_cycle\n";
    std::cout << "\nPress Ctrl+C to stop...\n";
    std::cout << "==========================================================\n\n";
    
    // Energy tracking
    double total_drive_energy_kwh = 0.0;
    double total_regen_energy_kwh = 0.0;
    double total_loss_energy_kwh = 0.0;
    
    // Control loop timing
    const double dt = 0.1;  // 100ms update rate (10Hz)
    auto last_update = std::chrono::steady_clock::now();
    double sim_time = 0.0;
    int iteration = 0;
    
    // Main control loop
    while (g_running.load()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
        
        if (elapsed.count() >= 100) {  // 100ms = 10Hz
            last_update = now;
            sim_time += dt;
            iteration++;
            
            // Simulate vehicle state
            VehicleState vehicle = simulate_vehicle(sim_time);
            
            // Simulate battery sensors
            BatterySensors battery = simulate_battery(sim_time, vehicle);
            
            // Update DS middleware with sensor data
            auto enhanced_state = middleware.enhance_cycle(
                battery.voltage,
                battery.current,
                battery.temperature,
                battery.soc,
                dt
            );
            
            // Get diagnostics
            auto diagnostics = middleware.get_diagnostics();
            
            // Compute torque limits
            auto torque_result = torque_mgr.compute_torque_limit(
                enhanced_state,
                vehicle.motor_rpm
            );
            
            // Compute regen limits
            auto regen_result = regen_mgr.compute_regen_limit(
                enhanced_state,
                &diagnostics,
                vehicle.brake_position,
                vehicle.speed_kph,
                vehicle.motor_rpm,
                vehicle.abs_active,
                false,  // wheel_slip
                dt
            );
            
            // Update energy tracking
            double power_kw = battery.voltage * battery.current / 1000.0;
            if (power_kw < 0.0) {
                total_drive_energy_kwh += (-power_kw) * (dt / 3600.0);
            } else {
                total_regen_energy_kwh += power_kw * (dt / 3600.0);
            }
            
            double efficiency = (total_drive_energy_kwh > 0.0) ?
                (total_regen_energy_kwh / total_drive_energy_kwh) * 100.0 : 0.0;
            
            // Update API server with latest state
            api_server.update_state(enhanced_state, torque_result, regen_result, diagnostics);
            api_server.update_energy_cycle(
                total_drive_energy_kwh,
                total_regen_energy_kwh,
                total_loss_energy_kwh,
                enhanced_state.state.energy_phi,
                enhanced_state.state.cycle_count,
                efficiency
            );
            
            // Print status every 50 iterations (5 seconds)
            if (iteration % 50 == 0) {
                std::cout << std::fixed << std::setprecision(2);
                std::cout << "t=" << std::setw(6) << sim_time << "s | "
                          << "SOC=" << std::setw(5) << (battery.soc * 100.0) << "% | "
                          << "Speed=" << std::setw(5) << vehicle.speed_kph << " kph | "
                          << "Torque=" << std::setw(6) << torque_result.max_drive_torque_nm << " Nm | "
                          << "Regen=" << std::setw(6) << regen_result.max_regen_torque_nm << " Nm | "
                          << "API requests=" << api_server.get_stats().total_requests << "\n";
            }
        }
        
        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Shutdown
    std::cout << "\nShutting down REST API server...\n";
    api_server.stop();
    
    std::cout << "\n==========================================================\n";
    std::cout << "Final Statistics:\n";
    std::cout << "==========================================================\n";
    std::cout << "Total simulation time: " << sim_time << " seconds\n";
    std::cout << "Total iterations: " << iteration << "\n";
    
    auto stats = api_server.get_stats();
    std::cout << "\nAPI Server Statistics:\n";
    std::cout << "  Total requests:      " << stats.total_requests << "\n";
    std::cout << "  Successful requests: " << stats.successful_requests << "\n";
    std::cout << "  Failed requests:     " << stats.failed_requests << "\n";
    
    std::cout << "\nEnergy Statistics:\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  Drive energy:  " << total_drive_energy_kwh << " kWh\n";
    std::cout << "  Regen energy:  " << total_regen_energy_kwh << " kWh\n";
    std::cout << "  Efficiency:    " << (total_regen_energy_kwh / std::max(0.001, total_drive_energy_kwh) * 100.0) << " %\n";
    
    std::cout << "\nServer stopped successfully.\n";
    return 0;
}
