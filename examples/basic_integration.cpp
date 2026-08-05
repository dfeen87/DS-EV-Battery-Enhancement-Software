/*
 * ============================================================================
 * DS BATTERY ENHANCEMENT - BASIC INTEGRATION EXAMPLE
 * ============================================================================
 * 
 * This file demonstrates the simplest possible integration of DS into
 * an existing Battery Management System.
 * 
 * Compile:
 *   g++ -std=c++17 -O3 -I../include examples/basic_integration.cpp -o basic_demo
 * 
 * Run:
 *   ./basic_demo
 * 
 * ============================================================================
 */

#include "ds_battery_enhancement.hpp"
#include "ds_bms_middleware_v2.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

// ============================================================================
// SIMULATED BMS SENSOR READINGS
// ============================================================================

struct BMSSensors {
    double voltage;
    double current;
    double temperature;
    double state_of_charge;
};

// Simulate a simple charge cycle
BMSSensors simulate_sensors(double time_in_cycle) {
    BMSSensors sensors;
    
    // Simulate charging phase (0-50%) then discharging (50-100%)
    double phase = std::fmod(time_in_cycle, 1.0);
    
    if (phase < 0.5) {
        // Charging
        sensors.state_of_charge = phase * 2.0; // 0 to 1
        sensors.current = 50.0; // 50A charging
        sensors.voltage = 350.0 + 50.0 * sensors.state_of_charge;
        sensors.temperature = 25.0 + 10.0 * phase * 2.0; // Heats up while charging
    } else {
        // Discharging
        sensors.state_of_charge = 2.0 - phase * 2.0; // 1 to 0
        sensors.current = -75.0; // 75A discharging
        sensors.voltage = 350.0 + 50.0 * sensors.state_of_charge;
        sensors.temperature = 30.0 + 5.0 * (1.0 - sensors.state_of_charge); // Heats up while discharging
    }
    
    return sensors;
}

// ============================================================================
// EXAMPLE 1: DROP-IN INTEGRATION (Minimal Code Changes)
// ============================================================================

void example_1_drop_in_integration() {
    std::cout << "\n=== EXAMPLE 1: Drop-In Integration ===\n\n";
    
    // Initialize DS middleware
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0); // 75Ah, 400V battery
    
    std::cout << "Running 10 charge cycles with DS enhancement...\n\n";
    
    // Simulate 10 cycles
    for (int cycle = 0; cycle < 10; ++cycle) {
        // Each cycle has 100 time steps
        for (int step = 0; step < 100; ++step) {
            double time = (cycle + step / 100.0);
            
            // Read sensors (THIS IS YOUR EXISTING BMS CODE)
            auto sensors = simulate_sensors(time);
            
            // ADD JUST THIS ONE LINE - Get DS enhancement
            auto enhanced = middleware.enhance_cycle(
                sensors.voltage,
                sensors.current,
                sensors.temperature,
                sensors.state_of_charge,
                0.1 // 0.1 second update interval
            );
            
            // Now use enhanced predictions in your BMS logic
            if (enhanced.degradation_warning) {
                // Trigger maintenance alert
                std::cout << "⚠️  Degradation warning at cycle " << cycle << "\n";
            }
            
            // Use optimal charging parameters
            double optimal_current = enhanced.charging.recommended_current_limit;
            double optimal_voltage = enhanced.charging.recommended_voltage_limit;
            
            // Your existing BMS continues normally...
            (void)optimal_current; // Suppress unused warning
            (void)optimal_voltage;
        }
        
        // Print cycle summary
        auto health = middleware.get_health_forecast(100.0);
        std::cout << "Cycle " << cycle << ": " 
                  << "Health = " << std::fixed << std::setprecision(1)
                  << health.remaining_capacity_percent << "%, "
                  << "Cycles to 80% = " << (int)health.cycles_to_80_percent << "\n";
    }
}

// ============================================================================
// EXAMPLE 2: USING DIAGNOSTICS
// ============================================================================

void example_2_diagnostics() {
    std::cout << "\n=== EXAMPLE 2: Using Diagnostics ===\n\n";
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0);
    
    // Run a few cycles
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (int step = 0; step < 100; ++step) {
            auto sensors = simulate_sensors(cycle + step / 100.0);
            middleware.enhance_cycle(
                sensors.voltage, sensors.current, 
                sensors.temperature, sensors.state_of_charge, 0.1
            );
        }
    }
    
    // Get comprehensive diagnostics
    auto diag = middleware.get_diagnostics();
    
    std::cout << "Diagnostic Report:\n";
    std::cout << "  Update Time: " << diag.last_update_time_ms << " ms (avg: " 
              << diag.average_update_time_ms << " ms)\n";
    std::cout << "  Pack Health: " << diag.pack_health_percent << "%\n";
    std::cout << "  Remaining Cycles: " << diag.estimated_remaining_cycles << "\n";
    std::cout << "  Degradation Rate: " << diag.degradation_rate_per_cycle << " per cycle\n";
    std::cout << "  Metric Trace: " << diag.metric_trace << "\n";
    std::cout << "  Phi Magnitude: " << diag.phi_magnitude << "\n";
    std::cout << "  Entropy Level: " << diag.entropy_level << "\n";
    std::cout << "  DS Confidence: " << diag.ds_confidence * 100 << "%\n";
    
    // Check warnings
    std::cout << "\nWarning Status:\n";
    std::cout << "  Degradation: " << (diag.degradation_warning ? "⚠️  YES" : "✓ OK") << "\n";
    std::cout << "  Thermal: " << (diag.thermal_warning ? "⚠️  YES" : "✓ OK") << "\n";
    std::cout << "  Weak Cells: " << (diag.weak_cell_warning ? "⚠️  YES" : "✓ OK") << "\n";
    
    // Print status summary
    std::cout << "\n" << middleware.get_status_summary() << "\n";
}

// ============================================================================
// EXAMPLE 3: CHEMISTRY-SPECIFIC OPTIMIZATION
// ============================================================================

void example_3_chemistry_optimization() {
    std::cout << "\n=== EXAMPLE 3: Chemistry-Specific Optimization ===\n\n";
    
    // Test different chemistries
    std::vector<ds::advanced::ChemistryType> chemistries = {
        ds::advanced::ChemistryType::LFP,
        ds::advanced::ChemistryType::NMC,
        ds::advanced::ChemistryType::NCA,
        ds::advanced::ChemistryType::LTO
    };
    
    std::vector<std::string> names = {"LFP", "NMC", "NCA", "LTO"};
    
    for (size_t i = 0; i < chemistries.size(); ++i) {
        ds_plugin::MiddlewareConfig config;
        config.chemistry = chemistries[i];
        config.nominal_capacity_ah = 75.0;
        config.nominal_voltage = 400.0;
        config.mode = ds_plugin::MiddlewareConfig::Mode::SINGLE_BATTERY;
        
        ds_plugin::DSBMSMiddleware middleware;
        middleware.init_advanced(config);
        
        // Run 100 cycles
        for (int cycle = 0; cycle < 100; ++cycle) {
            for (int step = 0; step < 100; ++step) {
                auto sensors = simulate_sensors(cycle + step / 100.0);
                middleware.enhance_cycle(
                    sensors.voltage, sensors.current,
                    sensors.temperature, sensors.state_of_charge, 0.1
                );
            }
        }
        
        auto health = middleware.get_health_forecast(1000.0);
        std::cout << names[i] << " Battery after 100 cycles:\n";
        std::cout << "  Health: " << std::fixed << std::setprecision(2)
                  << health.remaining_capacity_percent << "%\n";
        std::cout << "  Cycles to 80%: " << (int)health.cycles_to_80_percent << "\n";
        std::cout << "  Years to 80%: " << std::setprecision(1) 
                  << health.time_to_80_percent_years << "\n\n";
    }
}

// ============================================================================
// EXAMPLE 4: OPTIMAL CHARGING RECOMMENDATIONS
// ============================================================================

void example_4_optimal_charging() {
    std::cout << "\n=== EXAMPLE 4: Optimal Charging Recommendations ===\n\n";
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0);
    
    // Simulate battery at different states of charge
    std::vector<double> soc_levels = {0.1, 0.3, 0.5, 0.7, 0.9};
    
    std::cout << "Optimal charging parameters at different SoC levels:\n\n";
    std::cout << std::setw(6) << "SoC"
              << std::setw(12) << "Current(A)"
              << std::setw(12) << "Voltage(V)"
              << std::setw(12) << "Temp(°C)"
              << std::setw(15) << "Time(min)\n";
    std::cout << std::string(55, '-') << "\n";
    
    for (double soc : soc_levels) {
        auto sensors = simulate_sensors(soc);
        
        auto enhanced = middleware.enhance_cycle(
            sensors.voltage, sensors.current,
            sensors.temperature, soc, 0.1
        );
        
        auto charging = enhanced.charging;
        
        std::cout << std::fixed << std::setprecision(1)
                  << std::setw(6) << soc * 100 << "%"
                  << std::setw(12) << charging.recommended_current_limit
                  << std::setw(12) << charging.recommended_voltage_limit
                  << std::setw(12) << charging.recommended_temperature
                  << std::setw(15) << charging.estimated_charge_time << "\n";
    }
}

// ============================================================================
// EXAMPLE 5: LONG-TERM DEGRADATION FORECAST
// ============================================================================

void example_5_degradation_forecast() {
    std::cout << "\n=== EXAMPLE 5: Long-Term Degradation Forecast ===\n\n";
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0);
    
    // Age the battery through 500 cycles
    std::cout << "Aging battery through 500 cycles...\n\n";
    
    for (int cycle = 0; cycle < 500; ++cycle) {
        for (int step = 0; step < 100; ++step) {
            auto sensors = simulate_sensors(cycle + step / 100.0);
            middleware.enhance_cycle(
                sensors.voltage, sensors.current,
                sensors.temperature, sensors.state_of_charge, 0.1
            );
        }
        
        // Print forecast every 100 cycles
        if (cycle % 100 == 0) {
            auto health = middleware.get_health_forecast(500.0);
            
            std::cout << "After " << cycle << " cycles:\n";
            std::cout << "  Current Health: " << std::fixed << std::setprecision(2)
                      << health.remaining_capacity_percent << "%\n";
            std::cout << "  Predicted in 500 cycles: " 
                      << health.remaining_capacity_percent << "%\n";
            std::cout << "  Degradation Rate: " << std::scientific << std::setprecision(2)
                      << health.degradation_rate << " per cycle\n";
            std::cout << "  Confidence: " << std::fixed << std::setprecision(1)
                      << health.confidence * 100 << "%\n\n";
        }
    }
}

// ============================================================================
// MAIN - RUN ALL EXAMPLES
// ============================================================================

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS BATTERY ENHANCEMENT - INTEGRATION EXAMPLES\n";
    std::cout << "Version: " << ds::DSEnhancement::get_version() << "\n";
    std::cout << "============================================================================\n";
    
    try {
        example_1_drop_in_integration();
        example_2_diagnostics();
        example_3_chemistry_optimization();
        example_4_optimal_charging();
        example_5_degradation_forecast();
        
        std::cout << "\n============================================================================\n";
        std::cout << "All examples completed successfully!\n";
        std::cout << "============================================================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

/*
 * ============================================================================
 * COMPILATION AND USAGE
 * ============================================================================
 * 
 * Compile:
 *   g++ -std=c++17 -O3 -I../include examples/basic_integration.cpp -o basic_demo
 * 
 * Run:
 *   ./basic_demo
 * 
 * Expected Output:
 *   - Example 1: Shows simple drop-in integration
 *   - Example 2: Displays diagnostic information
 *   - Example 3: Compares different battery chemistries
 *   - Example 4: Shows optimal charging at different SoC levels
 *   - Example 5: Long-term degradation forecasting
 * 
 * ============================================================================
 */
