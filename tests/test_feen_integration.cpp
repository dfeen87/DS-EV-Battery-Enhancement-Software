#include "ds_battery_core.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <limits>

using namespace ds;

bool test_feen_disabled() {
    std::cout << "Testing FEEN disabled (pre-4.2.0 behavior)..." << std::flush;

    DSEnhancement enhancer;
    DSConfig config;
    config.enable_feen_battery_integration = false;
    enhancer.init(config);

    auto result = enhancer.enhance(360.0, 50.0, 25.0, 0.8, 1.0);
    (void)result;

    assert(result.feen_trust_metric == -1.0);
    assert(result.ds_confidence >= 0.0 && result.ds_confidence <= 1.0);

    std::cout << " PASS\n";
    return true;
}

bool test_feen_enabled() {
    std::cout << "Testing FEEN enabled..." << std::flush;

    DSEnhancement enhancer;
    DSConfig config;
    config.enable_feen_battery_integration = true;
    enhancer.init(config);

    auto result = enhancer.enhance(360.0, 50.0, 25.0, 0.8, 1.0);

    assert(result.feen_trust_metric >= 0.0 && result.feen_trust_metric <= 1.0);
    assert(result.ds_confidence >= 0.0 && result.ds_confidence <= 1.0);

    // Test multiple cycles
    for(int i = 0; i < 5; i++) {
        result = enhancer.enhance(360.0 + i, 50.0, 25.0, 0.8, 1.0);
    }
    assert(result.feen_trust_metric >= 0.0 && result.feen_trust_metric <= 1.0);

    std::cout << " PASS\n";
    return true;
}

bool test_feen_adapter_edge_cases() {
    std::cout << "Testing FEEN adapter edge cases..." << std::flush;

    BatteryFeenAdapter adapter;

    // Test normal value
    double normal_trust = adapter.compute_battery_trust_from_feen(360.0);
    (void)normal_trust;
    assert(normal_trust >= 0.0 && normal_trust <= 1.0);

    // Test NaN
    double nan_trust = adapter.compute_battery_trust_from_feen(std::nan(""));
    (void)nan_trust;
    assert(nan_trust >= 0.0 && nan_trust <= 1.0);

    // Test Inf
    double inf_trust = adapter.compute_battery_trust_from_feen(std::numeric_limits<double>::infinity());
    (void)inf_trust;
    assert(inf_trust >= 0.0 && inf_trust <= 1.0);

    std::cout << " PASS\n";
    return true;
}

bool test_determinism() {
    std::cout << "Testing determinism & replay..." << std::flush;

    DSConfig config;
    config.enable_feen_battery_integration = true;

    DSEnhancement enhancer1;
    enhancer1.init(config);
    auto r1 = enhancer1.enhance(360.0, 50.0, 25.0, 0.8, 1.0);
    (void)r1;

    DSEnhancement enhancer2;
    enhancer2.init(config);
    auto r2 = enhancer2.enhance(360.0, 50.0, 25.0, 0.8, 1.0);
    (void)r2;

    assert(r1.feen_trust_metric == r2.feen_trust_metric);
    assert(r1.ds_confidence == r2.ds_confidence);

    std::cout << " PASS\n";
    return true;
}

bool test_version_consistency() {
    std::cout << "Testing version consistency..." << std::flush;

    // Check main version string against the library's source of truth
    assert(DSEnhancement::get_version() == get_version_string());

    std::cout << " PASS\n";
    return true;
}

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS BATTERY FEEN INTEGRATION TESTS\n";
    std::cout << "============================================================================\n\n";

    try {
        bool all_passed = true;

        all_passed &= test_feen_disabled();
        all_passed &= test_feen_enabled();
        all_passed &= test_feen_adapter_edge_cases();
        all_passed &= test_determinism();
        all_passed &= test_version_consistency();

        std::cout << "\n============================================================================\n";
        if (all_passed) {
            std::cout << "✓ All FEEN integration tests PASSED\n";
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
