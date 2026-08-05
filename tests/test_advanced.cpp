/*
 * ============================================================================
 * DS BATTERY ENHANCEMENT - ADVANCED FEATURE TESTS
 * ============================================================================
 * 
 * Tests for advanced features: multi-cell packs, Kalman filtering, etc.
 * 
 * ============================================================================
 */

#include "ds_advanced_features.hpp"
#include "ds_battery_enhancement.hpp"
#include <iostream>
#include <cassert>

bool test_multi_cell_pack() {
    std::cout << "Testing multi-cell pack..." << std::flush;
    
    auto profile = ds::advanced::ChemistryLibrary().get_profile(
        ds::advanced::ChemistryType::NMC);
    
    ds::advanced::MultiCellPack pack(96, profile);
    
    std::vector<double> voltages(96, 3.7);
    std::vector<double> temps(96, 25.0);
    double current = 50.0;
    double dt = 1.0;
    
    pack.update_all_cells(voltages, temps, current, dt);
    
    auto cells = pack.get_cells();
    assert(cells.size() == 96);
    
    std::cout << " PASS\n";
    return true;
}

bool test_kalman_filter() {
    std::cout << "Testing Kalman filter..." << std::flush;
    
    ds::advanced::KalmanDSFilter filter;
    
    ds::DSState state;
    state.state_of_charge = 0.8;
    state.degradation = 0.0;
    
    filter.predict(state, 1.0);
    filter.update(0.8, 50.0);
    
    auto filtered_state = filter.get_state();
    (void)filtered_state;
    assert(filtered_state[0] >= 0.0 && filtered_state[0] <= 1.0);
    
    std::cout << " PASS\n";
    return true;
}

bool test_chemistry_profiles() {
    std::cout << "Testing chemistry profiles..." << std::flush;
    
    auto lib = ds::advanced::ChemistryLibrary();
    
    auto lfp = lib.get_profile(ds::advanced::ChemistryType::LFP);
    auto nmc = lib.get_profile(ds::advanced::ChemistryType::NMC);
    auto nca = lib.get_profile(ds::advanced::ChemistryType::NCA);
    
    assert(lfp.name == "Lithium Iron Phosphate");
    assert(nmc.name == "Nickel Manganese Cobalt");
    assert(nca.name == "Nickel Cobalt Aluminum");
    
    assert(lfp.nominal_voltage_per_cell < nmc.nominal_voltage_per_cell);
    
    std::cout << " PASS\n";
    return true;
}

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS ADVANCED FEATURE TESTS\n";
    std::cout << "============================================================================\n\n";
    
    try {
        bool all_passed = true;
        
        all_passed &= test_multi_cell_pack();
        all_passed &= test_kalman_filter();
        all_passed &= test_chemistry_profiles();
        
        std::cout << "\n============================================================================\n";
        if (all_passed) {
            std::cout << "✓ All advanced tests PASSED\n";
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
