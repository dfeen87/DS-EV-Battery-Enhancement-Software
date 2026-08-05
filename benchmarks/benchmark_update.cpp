/*
 * ============================================================================
 * DS BATTERY ENHANCEMENT - UPDATE BENCHMARK
 * ============================================================================
 * 
 * Performance benchmark for DS update cycle
 * 
 * ============================================================================
 */

#include "ds_battery_enhancement.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS UPDATE CYCLE BENCHMARK\n";
    std::cout << "============================================================================\n\n";
    
    ds::DSEnhancement enhancer;
    ds::DSConfig config;
    enhancer.init(config);
    
    const int NUM_ITERATIONS = 100000;
    
    std::cout << "Running " << NUM_ITERATIONS << " update cycles...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        double voltage = 360.0 + (i % 10) * 2.0;
        double current = 50.0 + (i % 5) * 10.0;
        double temperature = 25.0 + (i % 3) * 2.0;
        double soc = 0.5 + 0.4 * std::sin(i * 0.001);
        double dt = 1.0;
        
        enhancer.enhance(voltage, current, temperature, soc, dt);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double total_time_ms = duration.count() / 1000.0;
    double avg_time_us = duration.count() / static_cast<double>(NUM_ITERATIONS);
    double updates_per_sec = NUM_ITERATIONS / (total_time_ms / 1000.0);
    
    std::cout << "\n============================================================================\n";
    std::cout << "BENCHMARK RESULTS\n";
    std::cout << "============================================================================\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total time:        " << total_time_ms << " ms\n";
    std::cout << "Average per cycle: " << avg_time_us << " μs\n";
    std::cout << "Updates per sec:   " << std::setprecision(0) << updates_per_sec << " Hz\n";
    std::cout << "============================================================================\n";
    
    // Performance targets
    if (avg_time_us < 100.0) {
        std::cout << "✓ EXCELLENT: Suitable for 10kHz+ control loops\n";
    } else if (avg_time_us < 1000.0) {
        std::cout << "✓ GOOD: Suitable for 1kHz control loops\n";
    } else {
        std::cout << "⚠  SLOW: May need optimization for real-time use\n";
    }
    
    return 0;
}
