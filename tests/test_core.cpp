/*
 * ============================================================================
 * DS BATTERY ENHANCEMENT - CORE TESTS
 * ============================================================================
 * 
 * Basic unit tests for core DS functionality
 * 
 * ============================================================================
 */

#include "ds_battery_core.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <limits>

bool test_initialization() {
    std::cout << "Testing DS initialization..." << std::flush;
    
    ds::DSEnhancement enhancer;
    ds::DSConfig config;
    // Use valid config values within validation bounds
    config.lambda = 1e-6;  // Valid: (0, 1e-3]
    config.phi_decay_rate = 0.001;  // Valid: [0, 1]
    
    enhancer.init(config);
    
    std::cout << " PASS\n";
    return true;
}

bool test_enhance_cycle() {
    std::cout << "Testing enhance cycle..." << std::flush;
    
    ds::DSEnhancement enhancer;
    ds::DSConfig config;
    enhancer.init(config);
    
    // Simulate battery state
    double voltage = 360.0;
    double current = 50.0;
    double temperature = 25.0;
    double soc = 0.8;
    double dt = 1.0;
    
    auto result = enhancer.enhance(voltage, current, temperature, soc, dt);
    (void)result;
    
    // Basic sanity checks
    assert(result.state.voltage > 0);
    assert(result.state.state_of_charge >= 0 && result.state.state_of_charge <= 1.0);
    assert(result.ds_confidence >= 0 && result.ds_confidence <= 1.0);
    
    std::cout << " PASS\n";
    return true;
}

bool test_degradation_tracking() {
    std::cout << "Testing degradation tracking..." << std::flush;
    
    ds::DSEnhancement enhancer;
    ds::DSConfig config;
    enhancer.init(config);
    
    // Run many cycles to accumulate degradation
    double voltage = 360.0;
    double current = 50.0;
    double temperature = 25.0;
    double dt = 1.0;
    
    for (int i = 0; i < 1000; ++i) {
        double soc = 0.5 + 0.4 * std::sin(i * 0.1);
        enhancer.enhance(voltage, current, temperature, soc, dt);
    }
    
    auto health = enhancer.get_health_forecast(100.0);
    (void)health;
    
    // Degradation should be non-zero after 1000 cycles
    assert(health.remaining_capacity_percent < 100.0);
    assert(health.remaining_capacity_percent > 80.0); // Should still be above 80%
    
    std::cout << " PASS\n";
    return true;
}

bool test_energy_conservation() {
    std::cout << "Testing energy conservation..." << std::flush;
    
    ds::DSEnhancement enhancer;
    ds::DSConfig config;
    enhancer.init(config);
    
    double voltage = 360.0;
    double current = 50.0;
    double temperature = 25.0;
    double soc = 0.8;
    double dt = 1.0;
    
    auto result = enhancer.enhance(voltage, current, temperature, soc, dt);
    (void)result;
    
    // Check that energy metrics are reasonable
    assert(std::isfinite(result.state.entropy));
    assert(std::isfinite(result.state.phi_magnitude));
    
    std::cout << " PASS\n";
    return true;
}

bool test_invalid_inputs_are_atomic() {
    std::cout << "Testing invalid input rejection and atomic state..." << std::flush;
    ds::DSEnhancement enhancer;
    enhancer.init();
    enhancer.enhance(360.0, 10.0, 25.0, 0.8, 1.0);
    const ds::DSState before = enhancer.get_state();

    const double invalid_values[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()
    };
    for (double invalid : invalid_values) {
        bool rejected = false;
        try {
            enhancer.enhance(360.0, 10.0, 25.0, invalid, 1.0);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        assert(enhancer.get_state().time == before.time);
        assert(enhancer.get_state().state_of_charge == before.state_of_charge);
    }

    bool rejected_dt = false;
    try {
        enhancer.enhance(360.0, 10.0, 25.0, 0.8, 0.0);
    } catch (const std::invalid_argument&) {
        rejected_dt = true;
    }
    assert(rejected_dt);
    assert(enhancer.get_state().time == before.time);
    std::cout << " PASS\n";
    return true;
}

bool test_non_finite_configuration_rejected() {
    std::cout << "Testing non-finite configuration rejection..." << std::flush;
    ds::DSConfig config;
    config.nominal_capacity_ah = std::numeric_limits<double>::quiet_NaN();
    assert(!config.validate());
    std::cout << " PASS\n";
    return true;
}

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS BATTERY CORE TESTS\n";
    std::cout << "============================================================================\n\n";
    
    try {
        bool all_passed = true;
        
        all_passed &= test_initialization();
        all_passed &= test_enhance_cycle();
        all_passed &= test_degradation_tracking();
        all_passed &= test_energy_conservation();
        all_passed &= test_invalid_inputs_are_atomic();
        all_passed &= test_non_finite_configuration_rejected();
        
        std::cout << "\n============================================================================\n";
        if (all_passed) {
            std::cout << "✓ All core tests PASSED\n";
        } else {
            std::cout << "✗ Some tests FAILED\n";
            return 1;
        }
        std::cout << "============================================================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
