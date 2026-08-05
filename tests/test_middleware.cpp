/*
 * ============================================================================
 * DS BATTERY ENHANCEMENT - MIDDLEWARE TESTS
 * ============================================================================
 * 
 * Tests for BMS middleware integration layer
 * 
 * ============================================================================
 */

#include "ds_bms_middleware_v2.hpp"
#include <iostream>
#include <cassert>

bool test_middleware_initialization() {
    std::cout << "Testing middleware initialization..." << std::flush;
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0);
    
    std::cout << " PASS\n";
    return true;
}

bool test_middleware_enhance_cycle() {
    std::cout << "Testing middleware enhance cycle..." << std::flush;
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0);
    
    auto result = middleware.enhance_cycle(360.0, 50.0, 25.0, 0.8, 1.0);
    (void)result;
    
    assert(result.state.voltage > 0);
    assert(result.ds_confidence >= 0 && result.ds_confidence <= 1.0);
    
    std::cout << " PASS\n";
    return true;
}

bool test_middleware_diagnostics() {
    std::cout << "Testing middleware diagnostics..." << std::flush;
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init(75.0, 400.0);
    
    middleware.enhance_cycle(360.0, 50.0, 25.0, 0.8, 1.0);
    
    auto diag = middleware.diagnostics();
    
    assert(diag.pack_soh_percent > 0);
    assert(diag.pack_soc_percent >= 0 && diag.pack_soc_percent <= 100);
    assert(diag.update_count > 0);
    
    std::cout << " PASS\n";
    return true;
}

bool test_middleware_safety() {
    std::cout << "Testing middleware safety monitoring..." << std::flush;
    
    ds_plugin::DSBMSMiddleware middleware;
    
    ds_plugin::MiddlewareConfig config;
    config.nominal_capacity_ah = 75.0;
    config.nominal_voltage = 400.0;
    config.enable_safety_monitoring = true;
    config.safety_limits.max_discharge_current = 100.0;
    
    middleware.init_advanced(config);
    
    // Normal operation - should pass
    middleware.enhance_cycle(360.0, 50.0, 25.0, 0.8, 1.0);
    
    auto diag = middleware.diagnostics();
    assert(!diag.safety_fault); // Should not have safety fault
    
    std::cout << " PASS\n";
    return true;
}

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS MIDDLEWARE TESTS\n";
    std::cout << "============================================================================\n\n";
    
    try {
        bool all_passed = true;
        
        all_passed &= test_middleware_initialization();
        all_passed &= test_middleware_enhance_cycle();
        all_passed &= test_middleware_diagnostics();
        all_passed &= test_middleware_safety();
        
        std::cout << "\n============================================================================\n";
        if (all_passed) {
            std::cout << "✓ All middleware tests PASSED\n";
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
