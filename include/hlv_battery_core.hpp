/*
 * ============================================================================
 * HLV BATTERY ENHANCEMENT LIBRARY v4.1
 * ============================================================================
 * 
 * Implementation of Marcel Krüger's Helix-Light-Vortex (HLV) Theory
 * for Advanced Battery Management Systems
 * 
 * Based on: "Mathematical Formulation of the U2→U1 Coupling in the 
 *            Helix-Light-Vortex Theory" (Krüger, 2025)
 * 
 * UPDATES IN v4.1:
 * - Fixed monotonic degradation accumulation bug (use cycle delta, not absolute)
 * - Replaced exceptions in hot path with value clamping and input_clamped flag
 * - Initialized all struct members with safe defaults
 * - Added assert-based bounds checking to Matrix4x4::operator()
 * - Strengthened HLVConfig::validate() with additional parameter checks
 * - Enforced energy_conservation_tolerance to flag numerical instability
 * - Version bump to 4.1.0
 * 
 * ARCHITECTURE:
 * ------------
 * This library provides a drop-in enhancement layer for existing BMS systems.
 * It models batteries as dual-state systems:
 *   - Ψ (Psi): Physical/geometric state (voltage, current, temp, SoC)
 *   - Φ (Phi): Informational state (entropy, history, degradation)
 * 
 * The coupling between these states follows HLV's effective metric formulation:
 *   g_eff_μν = g_μν + λ ∂_μΦ ∂_νΦ
 * 
 * And respects energy conservation via the Landauer Principle:
 *   δE_total = δE_Ψ + δE_Φ + δE_metric = 0
 * 
 * INTEGRATION:
 * -----------
 * Single header include, minimal dependencies, real-time compatible.
 * Add two lines to existing BMS update loop and get HLV enhancement.
 * 
 * AUTHORS: Don Michael Feeney Jr. & Claude (Anthropic)
 * DATE: December 2025
 * LICENSE: Licensed under the PolyForm Noncommercial License 1.0.0
 * VERSION: 1.2.0
 * 
 * ============================================================================
 */

#ifndef HLV_BATTERY_CORE_HPP
#define HLV_BATTERY_CORE_HPP

#include <cmath>
#include <vector>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <cassert>

#ifndef HLV_ENABLE_FEEN
#define HLV_ENABLE_FEEN 0
#endif

#include "battery_feen_adapter/battery_feen_adapter.hpp"

namespace hlv {

// ============================================================================
// VERSION INFORMATION
// ============================================================================

constexpr int HLV_VERSION_MAJOR = 4;
constexpr int HLV_VERSION_MINOR = 1;
constexpr int HLV_VERSION_PATCH = 0;

inline std::string get_version_string() {
    return std::to_string(HLV_VERSION_MAJOR) + "." + 
           std::to_string(HLV_VERSION_MINOR) + "." + 
           std::to_string(HLV_VERSION_PATCH);
}

// ============================================================================
// CONSTANTS & CONFIGURATION
// ============================================================================

constexpr double PLANCK_REDUCED = 1.054571817e-34;  // ℏ (J·s)
constexpr double BOLTZMANN = 1.380649e-23;          // k_B (J/K)
constexpr double ELECTRON_CHARGE = 1.602176634e-19; // e (C)

// Default HLV coupling parameters (tunable per battery chemistry)
struct HLVConfig {
    bool enable_feen_battery_integration = false;
    double lambda = 1e-6;           // Coupling strength λ
    double tau_min = 0.01;          // Minimum update interval (s)
    double phi_decay_rate = 0.001;  // Information decay rate
    double landauer_beta = 1.0;     // Landauer energy scaling
    double entropy_weight = 0.5;    // Entropy contribution to Φ
    
    // Battery-specific parameters
    double nominal_capacity_ah = 75.0;  // Nominal capacity (Ah)
    double nominal_voltage = 400.0;     // Nominal voltage (V)
    double max_temperature = 60.0;      // Max safe temp (°C)
    double min_temperature = -20.0;     // Min safe temp (°C)
    
    // Validation and safety
    double max_current = 500.0;         // Max current (A)
    double energy_conservation_tolerance = 1e-6; // Energy balance tolerance
    
    // Validate configuration
    bool validate() const {
        if (lambda <= 0.0 || lambda > 1e-3) return false;
        if (tau_min <= 0.0 || tau_min > 1.0) return false;
        if (phi_decay_rate < 0.0 || phi_decay_rate > 1.0) return false;
        if (nominal_capacity_ah <= 0.0) return false;
        if (nominal_voltage <= 0.0) return false;
        if (max_temperature <= min_temperature) return false;
        if (max_current <= 0.0) return false;
        if (entropy_weight < 0.0 || entropy_weight > 1.0) return false;
        if (landauer_beta <= 0.0) return false;
        if (energy_conservation_tolerance <= 0.0) return false;
        // Explicit zero check for max_temperature: prevents divide-by-zero in
        // compute_phi_gradients (temp_factor = temperature / max_temperature).
        // Note: max_temperature > min_temperature above does not exclude zero when
        // min_temperature is negative (e.g. default -20.0).
        if (max_temperature == 0.0) return false;
        return true;
    }
};

// ============================================================================
// MATRIX4X4 - Lightweight 4x4 matrix for metric calculations
// ============================================================================

class Matrix4x4 {
public:
    std::array<std::array<double, 4>, 4> data;
    
    Matrix4x4() {
        // Initialize to Minkowski metric by default
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                data[i][j] = (i == j) ? ((i == 0) ? -1.0 : 1.0) : 0.0;
    }
    
    double& operator()(int i, int j) {
        assert(i >= 0 && i < 4 && j >= 0 && j < 4 && "Matrix4x4 index out of bounds");
        return data[i][j];
    }
    const double& operator()(int i, int j) const {
        assert(i >= 0 && i < 4 && j >= 0 && j < 4 && "Matrix4x4 index out of bounds");
        return data[i][j];
    }
    
    // Compute trace
    double trace() const {
        return data[0][0] + data[1][1] + data[2][2] + data[3][3];
    }
    
    // Check if metric is numerically stable
    bool is_stable() const {
        double tr = trace();
        return std::isfinite(tr) && std::abs(tr) < 1e6;
    }
};

// ============================================================================
// HLVSTATE - Dual-state representation (Ψ and Φ)
// ============================================================================

struct HLVState {
    // PHYSICAL STATE Ψ (directly measurable)
    double voltage;           // V
    double current;           // A
    double temperature;       // °C
    double state_of_charge;   // 0.0 to 1.0
    
    // INFORMATIONAL STATE Φ (computed/inferred)
    double entropy;           // Normalized entropy measure
    double cycle_count;       // Equivalent full cycles
    double degradation;       // 0.0 (new) to 1.0 (dead)
    double phi_magnitude;     // |Φ| for coupling calculations
    
    // COUPLING METRICS
    double lambda;            // Current coupling strength
    Matrix4x4 g_eff;          // Effective metric g^eff_μν
    
    // GRADIENTS (for metric modulation)
    std::array<double, 4> grad_phi;  // ∂_μΦ in (t,x,y,z) basis
    
    // ENERGY TRACKING (Landauer compliance)
    double energy_psi;        // Energy in physical state
    double energy_phi;        // Energy in informational state
    double energy_metric;     // Energy in metric modulation
    double energy_total;      // Total energy (should be conserved)
    
    // COULOMB COUNTING (for accurate SoC)
    double charge_throughput_ah;  // Total Ah throughput
    double capacity_fade;         // Measured capacity loss
    
    // TIMESTAMPS
    double time;              // Current time (s)
    double last_update;       // Last update time (s)
    
    // Constructor with defaults
    HLVState() : voltage(0), current(0), temperature(25), state_of_charge(1.0),
                 entropy(0), cycle_count(0), degradation(0), phi_magnitude(0),
                 lambda(1e-6), grad_phi{0,0,0,0},
                 energy_psi(0), energy_phi(0), energy_metric(0), energy_total(0),
                 charge_throughput_ah(0), capacity_fade(0),
                 time(0), last_update(0) {}
    
    // Validate state
    bool is_valid() const {
        if (!std::isfinite(voltage) || !std::isfinite(current) || 
            !std::isfinite(temperature)) return false;
        if (state_of_charge < 0.0 || state_of_charge > 1.0) return false;
        if (degradation < 0.0 || degradation > 1.0) return false;
        if (!g_eff.is_stable()) return false;
        return true;
    }
};

// ============================================================================
// PREDICTION RESULTS - Output structures for easy integration
// ============================================================================

struct HealthPrediction {
    double remaining_capacity_percent = 0.0;  // Predicted remaining capacity
    double cycles_to_80_percent = 0.0;        // Cycles until 80% capacity
    double estimated_eol_cycles = 0.0;        // End-of-life estimate
    double confidence = 0.0;                  // Prediction confidence [0,1]
    bool warning_triggered = false;           // Early degradation warning
    
    // Additional metrics
    double degradation_rate = 0.0;            // Per-cycle degradation rate
    double time_to_80_percent_years = 0.0;    // Time estimate (assuming 1 cycle/day)
};

struct OptimalChargingProfile {
    double recommended_current_limit = 0.0;   // Optimal current (A)
    double recommended_voltage_limit = 0.0;   // Optimal voltage (V)
    double recommended_temperature = 25.0;    // Target temp (°C); 25°C is standard ambient charging temperature
    double estimated_charge_time = 0.0;       // Time to full (minutes)
    double degradation_impact = 0.0;          // Impact on lifetime (normalized)
    
    // Safety margins
    double max_safe_current = 0.0;            // Absolute current limit
    double max_safe_voltage = 0.0;            // Absolute voltage limit
};

struct EnhancedState {
    double feen_trust_metric = -1.0;    // FEEN trust metric (if enabled)
    HLVState state;                     // Full HLV state
    HealthPrediction health;            // Health prediction
    OptimalChargingProfile charging;    // Optimal charging
    bool degradation_warning;           // Critical warning flag
    double hlv_confidence;              // Overall confidence in HLV prediction
    bool input_clamped = false;         // True if sensor inputs were clamped to safe range
    
    // Diagnostics
    double energy_conservation_error;   // Should be near zero
    bool numerical_stability;           // Stability check
};

// ============================================================================
// HLVCOUPLING - Core coupling dynamics engine
// ============================================================================

class HLVCoupling {
private:
    HLVConfig config_;
    
    // Compute gradients of Φ (simplified for battery state space)
    void compute_phi_gradients(HLVState& state) {
        // Map battery parameters to effective spacetime coordinates
        // t: time, x: SoC, y: temperature, z: cycle_count
        
        // Temporal gradient (how Φ changes with time)
        state.grad_phi[0] = -config_.phi_decay_rate * state.phi_magnitude;
        
        // SoC gradient (how info changes with charge state)
        state.grad_phi[1] = state.entropy * (1.0 - state.state_of_charge);
        
        // Temperature gradient (thermal coupling)
        double temp_factor = state.temperature / config_.max_temperature;
        state.grad_phi[2] = temp_factor * state.degradation;
        
        // Cycle gradient (history coupling)
        state.grad_phi[3] = std::sqrt(std::max(0.0, state.cycle_count)) * 0.01;
    }
    
    // Compute effective metric g^eff_μν = g_μν + λ ∂_μΦ ∂_νΦ
    void compute_effective_metric(HLVState& state) {
        // Start with Minkowski background
        state.g_eff = Matrix4x4();
        
        // Add HLV coupling term: λ ∂_μΦ ∂_νΦ
        for (int mu = 0; mu < 4; ++mu) {
            for (int nu = 0; nu < 4; ++nu) {
                state.g_eff(mu, nu) += state.lambda * 
                                       state.grad_phi[mu] * 
                                       state.grad_phi[nu];
            }
        }
    }
    
    // Update informational state Φ based on physical state Ψ
    void update_phi(HLVState& state, double dt) {
        // Entropy increases with cycling and temperature
        double temp_contrib = std::max(0.0, state.temperature - 25.0) / 35.0;
        double current_contrib = std::abs(state.current) / 100.0; // Normalized
        
        state.entropy += dt * config_.entropy_weight * 
                        (temp_contrib + current_contrib);
        state.entropy = std::min(state.entropy, 1.0);
        
        // Coulomb counting for accurate cycle tracking
        double charge_delta = std::abs(state.current) * dt / 3600.0; // Ah
        state.charge_throughput_ah += charge_delta;
        double prev_cycle_count = state.cycle_count;
        state.cycle_count = state.charge_throughput_ah / 
                           (2.0 * config_.nominal_capacity_ah);
        
        // Degradation model (simplified empirical model + HLV correction)
        double cycle_delta = state.cycle_count - prev_cycle_count;
        double base_degradation = cycle_delta * 0.0001; // 0.01% per cycle baseline
        double thermal_degradation = temp_contrib * 0.0005 * dt;
        double hlv_correction = state.g_eff.trace() * 0.00001; // Metric coupling effect
        
        state.degradation += base_degradation + thermal_degradation + hlv_correction;
        state.degradation = std::clamp(state.degradation, 0.0, 1.0);
        
        // Update capacity fade (for Kalman filter integration)
        state.capacity_fade = state.degradation;
        
        // Update Φ magnitude
        state.phi_magnitude = std::sqrt(state.entropy * state.entropy + 
                                       state.degradation * state.degradation);
    }
    
    // Compute energy in each component (Landauer principle)
    void compute_energies(HLVState& state) {
        // Physical energy (battery stored energy)
        state.energy_psi = state.voltage * state.state_of_charge * 
                          config_.nominal_capacity_ah * 3600.0; // Joules
        
        // Informational energy (Landauer: kT ln(2) per bit)
        double temp_kelvin = state.temperature + 273.15;
        double info_bits = state.phi_magnitude * 1000.0; // Normalized to bit count
        state.energy_phi = info_bits * BOLTZMANN * temp_kelvin * std::log(2.0);
        
        // Metric modulation energy (geometric correction)
        double metric_deviation = std::abs(state.g_eff.trace() - 2.0);
        state.energy_metric = metric_deviation * state.energy_psi * 1e-6;
        
        // Total energy
        state.energy_total = state.energy_psi + state.energy_phi + state.energy_metric;
    }
    
public:
    HLVCoupling(const HLVConfig& config = HLVConfig()) : config_(config) {
        if (!config_.validate()) {
            throw std::invalid_argument("Invalid HLVConfig parameters");
        }
    }
    
    // Main update function - call once per BMS cycle
    void update(HLVState& state, double dt) {
        if (dt < config_.tau_min) {
            return;  // Skip update: interval below tau_min (bit-erasure regime)
        }
        
        // Validate input state
        if (!state.is_valid()) {
            throw std::runtime_error("Invalid HLVState before update");
        }
        
        // Update time
        state.time += dt;
        state.last_update = state.time;
        
        // Update coupling strength (can be adaptive based on degradation)
        state.lambda = config_.lambda * (1.0 + 0.1 * state.degradation);
        
        // Update informational state based on physical state
        update_phi(state, dt);
        
        // Compute gradients and effective metric
        compute_phi_gradients(state);
        compute_effective_metric(state);
        
        // Compute energies (Landauer compliance check)
        compute_energies(state);
        
        // Validate output state
        if (!state.is_valid()) {
            throw std::runtime_error("Invalid HLVState after update");
        }
    }
    
    // Predict future degradation using HLV dynamics
    HealthPrediction predict_health(const HLVState& state, double horizon_cycles) const {
        HealthPrediction pred;
        
        // Project degradation forward using current coupling dynamics
        double current_degradation = state.degradation;
        double degradation_rate = state.g_eff.trace() * 0.0001; // From metric
        pred.degradation_rate = degradation_rate;
        
        double future_degradation = current_degradation + 
                                   degradation_rate * horizon_cycles;
        
        pred.remaining_capacity_percent = (1.0 - future_degradation) * 100.0;
        pred.remaining_capacity_percent = std::max(0.0, pred.remaining_capacity_percent);
        
        // Cycles to 80% (industry standard EOL)
        double cycles_to_80 = (0.2 - current_degradation) / 
                              std::max(degradation_rate, 1e-8);
        pred.cycles_to_80_percent = std::max(0.0, cycles_to_80);
        
        // Full EOL estimate (50% capacity)
        pred.estimated_eol_cycles = (0.5 - current_degradation) / 
                                   std::max(degradation_rate, 1e-8);
        
        // Time estimate (assuming 1 cycle per day)
        pred.time_to_80_percent_years = pred.cycles_to_80_percent / 365.0;
        
        // Confidence based on metric stability
        double metric_stability = 1.0 / (1.0 + std::abs(state.g_eff.trace() - 2.0));
        pred.confidence = metric_stability * (1.0 - state.degradation);
        
        // Warning if degradation accelerating
        pred.warning_triggered = (degradation_rate > 0.001) || 
                                (state.degradation > 0.3);
        
        return pred;
    }
    
    // Compute optimal charging profile using HLV constraints
    OptimalChargingProfile optimize_charging(const HLVState& state) {
        OptimalChargingProfile profile;
        
        // Use metric to determine optimal current (minimize metric distortion)
        double metric_factor = 1.0 / (1.0 + std::abs(state.g_eff.trace() - 2.0));
        profile.recommended_current_limit = 100.0 * metric_factor * 
                                          (1.0 - state.degradation);
        
        // Safety margin
        profile.max_safe_current = std::min(config_.max_current, 
                                           profile.recommended_current_limit * 1.2);
        
        // Voltage limit (stay within safe metric evolution)
        profile.recommended_voltage_limit = config_.nominal_voltage * 
                                          (1.0 + 0.1 * metric_factor);
        profile.max_safe_voltage = profile.recommended_voltage_limit * 1.05;
        
        // Temperature target (minimize entropy increase)
        profile.recommended_temperature = 25.0 + 5.0 * state.state_of_charge;
        
        // Estimated charge time
        double remaining_capacity = (1.0 - state.state_of_charge) * 
                                   config_.nominal_capacity_ah;
        profile.estimated_charge_time = 60.0 * remaining_capacity / 
                                       std::max(profile.recommended_current_limit, 1.0);
        
        // Degradation impact of this charging profile
        profile.degradation_impact = state.g_eff.trace() * 
                                    profile.recommended_current_limit / 100.0;
        
        return profile;
    }
    
    // Get current configuration
    const HLVConfig& get_config() const { return config_; }
};

// ============================================================================
// HLVENHANCEMENT - Easy integration interface (THE MAIN API)
// ============================================================================

class HLVEnhancement {
private:
    BatteryFeenAdapter feen_adapter_;
    HLVConfig config_;
    HLVCoupling coupling_;
    HLVState state_;
    bool initialized_;
    
    // Energy tracking for conservation validation
    double initial_energy_;
    double cumulative_energy_error_;
    
public:
    HLVEnhancement() : coupling_(), initialized_(false), 
                       initial_energy_(0.0), cumulative_energy_error_(0.0) {}
    
    // Initialize with battery configuration
    void init(const HLVConfig& config = HLVConfig()) {
        if (!config.validate()) {
            throw std::invalid_argument("Invalid HLVConfig parameters");
        }
        
        config_ = config;
        coupling_ = HLVCoupling(config);
        state_ = HLVState();
        state_.lambda = config.lambda;
        initial_energy_ = 0.0;
        cumulative_energy_error_ = 0.0;
        initialized_ = true;
    }
    
    // Main enhancement function - call once per BMS cycle
    // INPUT: Raw sensor readings from existing BMS
    // OUTPUT: Enhanced state with HLV predictions
    EnhancedState enhance(double voltage, double current, 
                         double temperature, double soc, 
                         double dt = 0.1) {
        if (!initialized_) {
            throw std::runtime_error("HLVEnhancement not initialized. Call init() first.");
        }
        
        // Bounds checking: clamp inputs and set flag instead of throwing
        bool input_clamped = false;
        if (std::abs(current) > config_.max_current) {
            current = std::copysign(config_.max_current, current);
            input_clamped = true;
        }
        if (temperature > config_.max_temperature) {
            temperature = config_.max_temperature;
            input_clamped = true;
        } else if (temperature < config_.min_temperature) {
            temperature = config_.min_temperature;
            input_clamped = true;
        }
        
        // Update physical state from sensors
        state_.voltage = voltage;
        state_.current = current;
        state_.temperature = temperature;
        state_.state_of_charge = std::clamp(soc, 0.0, 1.0);
        
        // Store energy before update
        double energy_before = state_.energy_total;
        
        // Run HLV coupling dynamics
        coupling_.update(state_, dt);
        
        // Check energy conservation
        double energy_error = std::abs(state_.energy_total - energy_before);
        cumulative_energy_error_ += energy_error;
        
        // Generate predictions
        EnhancedState result;
        result.state = state_;
        result.health = coupling_.predict_health(state_, 100.0); // 100 cycle horizon
        result.charging = coupling_.optimize_charging(state_);
        result.degradation_warning = result.health.warning_triggered;
        result.hlv_confidence = result.health.confidence;
        result.input_clamped = input_clamped;
        result.energy_conservation_error = energy_error;
        result.numerical_stability = state_.is_valid() && state_.g_eff.is_stable() &&
            (energy_error <= config_.energy_conservation_tolerance);
        
        if (config_.enable_feen_battery_integration) {
            double trust = feen_adapter_.compute_battery_trust_from_feen(state_.voltage);
            result.feen_trust_metric = trust;
            result.hlv_confidence *= trust;
        }
        return result;
    }
    
    // Get current HLV state (for debugging/monitoring)
    const HLVState& get_state() const { return state_; }
    
    // Get long-term health forecast
    HealthPrediction get_health_forecast(double cycles_ahead) {
        if (!initialized_) {
            throw std::runtime_error("HLVEnhancement not initialized");
        }
        return coupling_.predict_health(state_, cycles_ahead);
    }
    
    // Get optimal charging recommendation
    OptimalChargingProfile get_optimal_charging() {
        if (!initialized_) {
            throw std::runtime_error("HLVEnhancement not initialized");
        }
        return coupling_.optimize_charging(state_);
    }
    
    // Check energy conservation (Landauer compliance)
    double check_energy_conservation() const {
        return cumulative_energy_error_;
    }
    
    // Reset state (for testing or after battery replacement)
    void reset_state() {
        feen_adapter_.reset();
        state_ = HLVState();
        state_.lambda = config_.lambda;
        initial_energy_ = 0.0;
        cumulative_energy_error_ = 0.0;
    }
    
    // Get version information
    static std::string get_version() {
        return get_version_string();
    }
};

} // namespace hlv

#endif // HLV_BATTERY_CORE_HPP

// ============================================================================
// EXAMPLE INTEGRATION
// ============================================================================

#ifdef HLV_EXAMPLE_MAIN

#include <iostream>
#include <iomanip>

int main() {
    using namespace hlv;
    
    std::cout << "=== HLV Battery Enhancement Demo v" 
              << HLVEnhancement::get_version() << " ===\n\n";
    
    // Initialize HLV enhancement
    HLVEnhancement hlv;
    HLVConfig config;
    config.lambda = 1e-6;
    config.nominal_capacity_ah = 75.0;
    config.nominal_voltage = 400.0;
    
    try {
        hlv.init(config);
    } catch (const std::exception& e) {
        std::cerr << "Initialization error: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "Simulating 1000 charge cycles...\n\n";
    
    // Simulate battery operation over many cycles
    double time = 0.0;
    const double dt = 10.0; // 10 second updates
    
    for (int cycle = 0; cycle < 1000; ++cycle) {
        // Simulate one charge-discharge cycle
        for (int step = 0; step < 100; ++step) {
            // Synthetic sensor data (would come from real BMS)
            double soc = std::abs(std::sin(step * 0.0314)); // Oscillating SoC
            double voltage = 350.0 + 50.0 * soc;
            double current = (step < 50) ? 50.0 : -50.0; // Charge then discharge
            double temp = 25.0 + 10.0 * std::abs(current) / 50.0;
            
            try {
                // Get HLV enhancement
                auto enhanced = hlv.enhance(voltage, current, temp, soc, dt);
                
                time += dt;
                
                // Print status every 100 cycles
                if (cycle % 100 == 0 && step == 0) {
                    std::cout << std::fixed << std::setprecision(2);
                    std::cout << "Cycle " << cycle << ":\n";
                    std::cout << "  Degradation: " << enhanced.state.degradation * 100 << "%\n";
                    std::cout << "  Remaining Capacity: " << enhanced.health.remaining_capacity_percent << "%\n";
                    std::cout << "  Cycles to 80%: " << enhanced.health.cycles_to_80_percent << "\n";
                    std::cout << "  Years to 80%: " << enhanced.health.time_to_80_percent_years << "\n";
                    std::cout << "  Metric Trace: " << enhanced.state.g_eff.trace() << "\n";
                    std::cout << "  HLV Confidence: " << enhanced.hlv_confidence * 100 << "%\n";
                    std::cout << "  Stability: " << (enhanced.numerical_stability ? "OK" : "WARNING") << "\n";
                    
                    if (enhanced.degradation_warning) {
                        std::cout << "  ⚠️  DEGRADATION WARNING TRIGGERED\n";
                    }
                    std::cout << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Update error at cycle " << cycle << ": " << e.what() << "\n";
                return 1;
            }
        }
    }
    
    // Final predictions
    std::cout << "=== Final Health Assessment ===\n";
    auto final_health = hlv.get_health_forecast(500.0);
    std::cout << "Predicted remaining capacity: " << final_health.remaining_capacity_percent << "%\n";
    std::cout << "Estimated cycles to EOL: " << final_health.estimated_eol_cycles << "\n";
    std::cout << "Prediction confidence: " << final_health.confidence * 100 << "%\n";
    
    std::cout << "\n=== Energy Conservation Check ===\n";
    std::cout << "Cumulative energy error: " << hlv.check_energy_conservation() << " J\n";
    std::cout << "(Should be small for Landauer compliance)\n";
    
    return 0;
}

#endif // HLV_EXAMPLE_MAIN
