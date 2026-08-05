/*
 * ============================================================================
 * DS BMS MIDDLEWARE PLUGIN v2.0
 * ============================================================================
 *
 * Enhanced middleware integrating both core DS and advanced features.
 * Provides a unified interface for EV Battery Management Systems.
 *
 * NEW in v2.0:
 *   - Chemistry-specific optimization
 *   - Multi-cell pack management
 *   - Kalman filtering for improved accuracy
 *   - ML hybrid predictions (optional)
 *   - Fleet learning contribution (opt-in)
 *   - Advanced health monitoring and diagnostics
 *
 * The middleware automatically selects the appropriate backend:
 *   - Single battery: Uses core DS enhancement
 *   - Multi-cell pack: Uses advanced features with per-cell tracking
 *
 * ============================================================================
 */

#ifndef DS_BMS_MIDDLEWARE_HPP
#define DS_BMS_MIDDLEWARE_HPP

#include "ds_battery_enhancement.hpp"
#include "ds_advanced_features.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>

namespace ds_plugin {

// ============================================================================
// CONFIGURATION STRUCTURE
// ============================================================================

struct MiddlewareConfig {
    // Battery specifications
    double nominal_capacity_ah = 75.0;
    double nominal_voltage = 400.0;
    int series_cells = 96;  // For multi-cell mode
    
    // Chemistry selection
    ds::advanced::ChemistryType chemistry = ds::advanced::ChemistryType::NMC;
    
    // Operating mode
    enum class Mode {
        SINGLE_BATTERY,  // Use core DS only
        MULTI_CELL_PACK  // Use advanced features
    } mode = Mode::SINGLE_BATTERY;
    
    // Advanced features (only used in MULTI_CELL_PACK mode)
    bool enable_kalman_filter = true;
    bool enable_ml_hybrid = false;
    bool enable_fleet_learning = false;
    
    // Performance tuning
    double update_rate_hz = 10.0;  // Target update frequency
    bool enable_diagnostics = true;
};

// ============================================================================
// DIAGNOSTIC INFORMATION
// ============================================================================

struct DiagnosticReport {
    // Timing information
    double last_update_time_ms = 0.0;
    double average_update_time_ms = 0.0;
    
    // Health metrics
    double pack_health_percent = 0.0;
    double estimated_remaining_cycles = 0.0;
    double degradation_rate_per_cycle = 0.0;
    
    // Multi-cell specific (empty if single battery mode)
    std::vector<int> weak_cell_ids;
    double voltage_imbalance_mv = 0.0;
    double temperature_spread_celsius = 0.0;
    
    // Warnings and alerts
    bool degradation_warning = false;
    bool thermal_warning = false;
    bool imbalance_warning = false;
    bool weak_cell_warning = false;
    
    // DS metrics
    double metric_trace = 0.0;
    double phi_magnitude = 0.0;
    double entropy_level = 0.0;
    double ds_confidence = 0.0;
};

// ============================================================================
// ENHANCED MIDDLEWARE CLASS
// ============================================================================

class DSBMSMiddleware {
private:
    // Configuration
    MiddlewareConfig config_;
    bool initialized_;
    
    // Core DS (for single battery mode)
    std::unique_ptr<ds::DSEnhancement> core_ds_;
    
    // Advanced features (for multi-cell pack mode)
    std::unique_ptr<ds::advanced::AdvancedDSSystem> advanced_system_;
    
    // Diagnostics
    DiagnosticReport last_diagnostic_;
    double total_update_time_ms_;
    int update_count_;

    ds::advanced::ChemistryLibrary chemistry_lib_;
    
    // Timing helpers
    double get_time_ms() const {
        // In production: use high-resolution timer
        return 0.0; // Placeholder
    }
    
    void update_diagnostics(double update_time_ms, 
                          const ds::EnhancedState* single_state = nullptr) {
        last_diagnostic_.last_update_time_ms = update_time_ms;
        total_update_time_ms_ += update_time_ms;
        update_count_++;
        last_diagnostic_.average_update_time_ms = 
            total_update_time_ms_ / update_count_;
        
        if (config_.mode == MiddlewareConfig::Mode::SINGLE_BATTERY && single_state) {
            // Single battery diagnostics
            last_diagnostic_.pack_health_percent = 
                single_state->health.remaining_capacity_percent;
            last_diagnostic_.estimated_remaining_cycles = 
                single_state->health.cycles_to_80_percent;
            last_diagnostic_.degradation_warning = 
                single_state->degradation_warning;
            last_diagnostic_.metric_trace = 
                single_state->state.g_eff.trace();
            last_diagnostic_.phi_magnitude = 
                single_state->state.phi_magnitude;
            last_diagnostic_.entropy_level = 
                single_state->state.entropy;
            last_diagnostic_.ds_confidence =
                single_state->ds_confidence;
            
            // No multi-cell warnings in single mode
            last_diagnostic_.weak_cell_ids.clear();
            last_diagnostic_.voltage_imbalance_mv = 0.0;
            last_diagnostic_.temperature_spread_celsius = 0.0;
            last_diagnostic_.imbalance_warning = false;
            last_diagnostic_.weak_cell_warning = false;
            
        } else if (config_.mode == MiddlewareConfig::Mode::MULTI_CELL_PACK) {
            // Multi-cell pack diagnostics
            auto health = advanced_system_->get_pack_health(100.0);
            last_diagnostic_.pack_health_percent = 
                health.remaining_capacity_percent;
            last_diagnostic_.estimated_remaining_cycles = 
                health.cycles_to_80_percent;
            last_diagnostic_.degradation_warning = 
                health.warning_triggered;
            
            // Cell-level diagnostics
            last_diagnostic_.weak_cell_ids = 
                advanced_system_->get_weak_cells();
            last_diagnostic_.weak_cell_warning = 
                !last_diagnostic_.weak_cell_ids.empty();
            
            last_diagnostic_.voltage_imbalance_mv =
                advanced_system_->get_voltage_imbalance() * 1000.0;
            last_diagnostic_.temperature_spread_celsius =
                advanced_system_->get_temperature_spread();
            last_diagnostic_.metric_trace = advanced_system_->get_metric_trace();
            last_diagnostic_.phi_magnitude = advanced_system_->get_phi_magnitude();
            last_diagnostic_.entropy_level = advanced_system_->get_entropy_level();
            last_diagnostic_.ds_confidence = advanced_system_->get_ml_confidence();
        }
        
        // Thermal warning (simplified - would use chemistry-specific limits)
        const auto& profile = chemistry_lib_.get_profile(config_.chemistry);
        if (config_.mode == MiddlewareConfig::Mode::SINGLE_BATTERY && single_state) {
            last_diagnostic_.thermal_warning =
                single_state->state.temperature >= profile.max_safe_temperature;
        } else if (config_.mode == MiddlewareConfig::Mode::MULTI_CELL_PACK) {
            last_diagnostic_.thermal_warning =
                advanced_system_->get_max_cell_temperature() >= profile.max_safe_temperature;
        } else {
            last_diagnostic_.thermal_warning = false;
        }
        
        // Imbalance warning for multi-cell packs
        if (config_.mode == MiddlewareConfig::Mode::MULTI_CELL_PACK) {
            last_diagnostic_.imbalance_warning = 
                (last_diagnostic_.voltage_imbalance_mv > 100.0); // >100mV
        }
    }
    
public:
    DSBMSMiddleware()
        : initialized_(false), 
          total_update_time_ms_(0.0), 
          update_count_(0) {}
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    // Simple initialization (single battery, NMC chemistry)
    void init(double nominal_capacity_ah, 
             double nominal_voltage,
             double max_temperature = 60.0) {
        config_.nominal_capacity_ah = nominal_capacity_ah;
        config_.nominal_voltage = nominal_voltage;
        config_.mode = MiddlewareConfig::Mode::SINGLE_BATTERY;
        config_.chemistry = ds::advanced::ChemistryType::NMC;
        
        // Initialize core DS
        ds::DSConfig ds_config;
        ds_config.nominal_capacity_ah = nominal_capacity_ah;
        ds_config.nominal_voltage = nominal_voltage;
        ds_config.max_temperature = max_temperature;
        
        core_ds_ = std::make_unique<ds::DSEnhancement>();
        core_ds_->init(ds_config);
        
        initialized_ = true;
    }
    
    // Advanced initialization (multi-cell pack with chemistry selection)
    void init_advanced(const MiddlewareConfig& config) {
        config_ = config;
        
        if (config_.mode == MiddlewareConfig::Mode::SINGLE_BATTERY) {
            // Use core DS with chemistry-optimized parameters
            ds::advanced::ChemistryLibrary chem_lib;
            auto ds_config = chem_lib.create_config(
                config_.chemistry,
                config_.nominal_capacity_ah,
                config_.series_cells
            );
            
            core_ds_ = std::make_unique<ds::DSEnhancement>();
            core_ds_->init(ds_config);
            
        } else {
            // Use advanced system for multi-cell packs
            advanced_system_ = std::make_unique<ds::advanced::AdvancedDSSystem>();
            advanced_system_->init(
                config_.chemistry,
                config_.nominal_capacity_ah,
                config_.series_cells,
                config_.enable_kalman_filter,
                config_.enable_ml_hybrid,
                config_.enable_fleet_learning
            );
        }
        
        initialized_ = true;
    }
    
    // ========================================================================
    // UPDATE METHODS
    // ========================================================================
    
    // Single battery update (legacy interface - backward compatible)
    ds::EnhancedState enhance_cycle(double voltage,
                                    double current,
                                    double temperature, 
                                    double soc,
                                    double dt = 0.1) {
        if (!initialized_) {
            throw std::runtime_error("DSBMSMiddleware not initialized.");
        }
        
        if (config_.mode != MiddlewareConfig::Mode::SINGLE_BATTERY) {
            throw std::runtime_error(
                "enhance_cycle() only available in SINGLE_BATTERY mode. "
                "Use update_pack() for multi-cell packs."
            );
        }
        
        double start_time = get_time_ms();
        
        // Call core DS
        auto result = core_ds_->enhance(voltage, current, temperature, soc, dt);
        
        double end_time = get_time_ms();
        
        if (config_.enable_diagnostics) {
            update_diagnostics(end_time - start_time, &result);
        }
        
        return result;
    }
    
    // Multi-cell pack update (new interface)
    void update_pack(const std::vector<double>& cell_voltages,
                    const std::vector<double>& cell_temperatures,
                    double pack_current,
                    double dt = 0.1) {
        if (!initialized_) {
            throw std::runtime_error("DSBMSMiddleware not initialized.");
        }
        
        if (config_.mode != MiddlewareConfig::Mode::MULTI_CELL_PACK) {
            throw std::runtime_error(
                "update_pack() only available in MULTI_CELL_PACK mode. "
                "Use enhance_cycle() for single battery."
            );
        }
        
        if (cell_voltages.size() != static_cast<size_t>(config_.series_cells) ||
            cell_temperatures.size() != static_cast<size_t>(config_.series_cells)) {
            throw std::runtime_error(
                "Cell data size mismatch. Expected " + 
                std::to_string(config_.series_cells) + " cells."
            );
        }
        
        double start_time = get_time_ms();
        
        // Call advanced system
        advanced_system_->update(cell_voltages, cell_temperatures, 
                                pack_current, dt);
        
        double end_time = get_time_ms();
        
        if (config_.enable_diagnostics) {
            update_diagnostics(end_time - start_time);
        }
    }
    
    // ========================================================================
    // QUERY METHODS
    // ========================================================================
    
    // Get health prediction
    ds::HealthPrediction get_health_forecast(double cycles_ahead = 100.0) {
        if (!initialized_) {
            throw std::runtime_error("DSBMSMiddleware not initialized.");
        }
        
        if (config_.mode == MiddlewareConfig::Mode::SINGLE_BATTERY) {
            return core_ds_->get_health_forecast(cycles_ahead);
        } else {
            return advanced_system_->get_pack_health(cycles_ahead);
        }
    }
    
    // Get optimal charging profile
    ds::OptimalChargingProfile get_optimal_charging() {
        if (!initialized_) {
            throw std::runtime_error("DSBMSMiddleware not initialized.");
        }
        
        if (config_.mode == MiddlewareConfig::Mode::SINGLE_BATTERY) {
            return core_ds_->get_optimal_charging();
        } else {
            const auto& chemistry_profile = chemistry_lib_.get_profile(config_.chemistry);
            ds::OptimalChargingProfile profile_out;

            const double max_current =
                chemistry_profile.max_charge_rate * config_.nominal_capacity_ah;
            profile_out.recommended_current_limit = max_current;
            profile_out.recommended_voltage_limit =
                chemistry_profile.max_voltage_per_cell * config_.series_cells;
            profile_out.recommended_temperature = chemistry_profile.optimal_temperature;
            profile_out.max_safe_current = max_current;
            profile_out.max_safe_voltage =
                chemistry_profile.max_voltage_per_cell * config_.series_cells;

            const double temperature_delta = std::abs(
                advanced_system_->get_average_temperature() -
                chemistry_profile.optimal_temperature);
            profile_out.degradation_impact = std::clamp(
                temperature_delta / chemistry_profile.max_safe_temperature, 0.0, 1.0);

            if (profile_out.recommended_current_limit > 0.0) {
                profile_out.estimated_charge_time =
                    (config_.nominal_capacity_ah / profile_out.recommended_current_limit) * 60.0;
            } else {
                profile_out.estimated_charge_time = 0.0;
            }
            return profile_out;
        }
    }
    
    // Get weak cells (multi-cell pack only)
    std::vector<int> get_weak_cells() const {
        if (config_.mode == MiddlewareConfig::Mode::MULTI_CELL_PACK) {
            return advanced_system_->get_weak_cells();
        }
        return {}; // Empty for single battery
    }
    
    // Get diagnostics report
    DiagnosticReport get_diagnostics() const {
        return last_diagnostic_;
    }
    
    // Get current configuration
    const MiddlewareConfig& get_config() const {
        return config_;
    }
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    // Check if specific warning is active
    bool has_degradation_warning() const {
        return last_diagnostic_.degradation_warning;
    }
    
    bool has_thermal_warning() const {
        return last_diagnostic_.thermal_warning;
    }
    
    bool has_weak_cells() const {
        return last_diagnostic_.weak_cell_warning;
    }
    
    bool has_imbalance_warning() const {
        return last_diagnostic_.imbalance_warning;
    }
    
    // Get summary status
    std::string get_status_summary() const {
        std::string status = "DS BMS Status:\n";
        status += "  Mode: " + std::string(
            config_.mode == MiddlewareConfig::Mode::SINGLE_BATTERY 
            ? "Single Battery" : "Multi-Cell Pack"
        ) + "\n";
        status += "  Health: " + 
            std::to_string(last_diagnostic_.pack_health_percent) + "%\n";
        status += "  Remaining Cycles: " + 
            std::to_string(last_diagnostic_.estimated_remaining_cycles) + "\n";
        
        if (config_.mode == MiddlewareConfig::Mode::MULTI_CELL_PACK) {
            status += "  Weak Cells: " + 
                std::to_string(last_diagnostic_.weak_cell_ids.size()) + "\n";
        }
        
        status += "  Update Time: " + 
            std::to_string(last_diagnostic_.last_update_time_ms) + " ms\n";
        
        if (last_diagnostic_.degradation_warning ||
            last_diagnostic_.thermal_warning ||
            last_diagnostic_.weak_cell_warning ||
            last_diagnostic_.imbalance_warning) {
            status += "  ⚠️  WARNINGS ACTIVE\n";
        }
        
        return status;
    }
    
    // Reset diagnostics counters
    void reset_diagnostics() {
        total_update_time_ms_ = 0.0;
        update_count_ = 0;
        last_diagnostic_ = DiagnosticReport();
    }
};

} // namespace ds_plugin

#endif // DS_BMS_MIDDLEWARE_HPP

/*
 * ============================================================================
 * EXAMPLE USAGE - SINGLE BATTERY MODE (Backward Compatible)
 * ============================================================================
 *
 * #include "ds_bms_middleware_v2.hpp"
 *
 * ds_plugin::DSBMSMiddleware middleware;
 * 
 * // Simple init (uses defaults)
 * middleware.init(75.0, 400.0);
 *
 * // In your BMS loop:
 * auto enhanced = middleware.enhance_cycle(voltage, current, temp, soc, dt);
 *
 * if (enhanced.degradation_warning) {
 *     trigger_maintenance_alert();
 * }
 *
 * ============================================================================
 * EXAMPLE USAGE - MULTI-CELL PACK MODE (New)
 * ============================================================================
 *
 * #include "ds_bms_middleware_v2.hpp"
 *
 * using namespace ds_plugin;
 *
 * // Configure for Tesla-style pack (96S NMC cells)
 * MiddlewareConfig config;
 * config.nominal_capacity_ah = 75.0;
 * config.nominal_voltage = 400.0;
 * config.series_cells = 96;
 * config.chemistry = ds::advanced::ChemistryType::NMC;
 * config.mode = MiddlewareConfig::Mode::MULTI_CELL_PACK;
 * config.enable_kalman_filter = true;
 * config.enable_ml_hybrid = false;  // Optional
 * config.enable_fleet_learning = false;  // Opt-in
 *
 * DSBMSMiddleware middleware;
 * middleware.init_advanced(config);
 *
 * // In your BMS loop:
 * std::vector<double> cell_voltages = read_all_cell_voltages();  // 96 values
 * std::vector<double> cell_temps = read_all_cell_temperatures(); // 96 values
 * double pack_current = read_pack_current();
 *
 * middleware.update_pack(cell_voltages, cell_temps, pack_current, 0.1);
 *
 * // Check for issues
 * if (middleware.has_weak_cells()) {
 *     auto weak_ids = middleware.get_weak_cells();
 *     for (int id : weak_ids) {
 *         log_weak_cell(id);
 *     }
 * }
 *
 * if (middleware.has_imbalance_warning()) {
 *     trigger_cell_balancing();
 * }
 *
 * // Get health forecast
 * auto health = middleware.get_health_forecast(500.0);
 * if (health.cycles_to_80_percent < 1000) {
 *     notify_user_warranty_expiring();
 * }
 *
 * // Print diagnostics
 * auto diag = middleware.get_diagnostics();
 * std::cout << "Update time: " << diag.last_update_time_ms << " ms\n";
 * std::cout << "Pack health: " << diag.pack_health_percent << "%\n";
 * std::cout << "Voltage imbalance: " << diag.voltage_imbalance_mv << " mV\n";
 *
 * ============================================================================
 * CHEMISTRY-SPECIFIC OPTIMIZATION EXAMPLE
 * ============================================================================
 *
 * // For LFP batteries (BYD Blade style)
 * config.chemistry = ds::advanced::ChemistryType::LFP;
 * config.series_cells = 120;  // Lower voltage per cell
 * config.nominal_voltage = 384.0;  // 3.2V × 120
 *
 * // For LTO batteries (fast-charging buses)
 * config.chemistry = ds::advanced::ChemistryType::LTO;
 * config.series_cells = 167;  // Even lower voltage per cell
 * config.nominal_voltage = 400.0;  // 2.4V × 167
 *
 * // For NCA batteries (Tesla Model S/X)
 * config.chemistry = ds::advanced::ChemistryType::NCA;
 * config.series_cells = 96;
 * config.nominal_voltage = 345.6;  // 3.6V × 96
 *
 * ============================================================================
 */
