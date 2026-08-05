/*
 * ============================================================================
 * DS BATTERY ENHANCEMENT - MULTI-CELL BENCHMARK
 * ============================================================================
 * 
 * Performance benchmark for multi-cell pack operations
 * 
 * ============================================================================
 */

#include "ds_advanced_features.hpp"
#include "ds_battery_enhancement.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS MULTI-CELL PACK BENCHMARK\n";
    std::cout << "============================================================================\n\n";
    
    const int NUM_CELLS = 96;
    const int NUM_ITERATIONS = 10000;
    
    auto profile = ds::advanced::ChemistryLibrary().get_profile(
        ds::advanced::ChemistryType::NMC);
    
    ds::advanced::MultiCellPack pack(NUM_CELLS, profile);
    
    std::vector<double> voltages(NUM_CELLS);
    std::vector<double> temps(NUM_CELLS);
    
    std::cout << "Benchmarking " << NUM_CELLS << "-cell pack with " 
              << NUM_ITERATIONS << " updates...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // Simulate cell variations
        for (int j = 0; j < NUM_CELLS; ++j) {
            voltages[j] = 3.7 + 0.1 * std::sin(i * 0.01 + j * 0.1);
            temps[j] = 25.0 + 5.0 * std::sin(i * 0.005 + j * 0.05);
        }
        
        double current = 50.0;
        double dt = 1.0;
        pack.update_all_cells(voltages, temps, current, dt);
        
        // Check for weak cells
        auto weak = pack.get_weak_cell_ids();
        (void)weak;
        auto health = pack.get_pack_health(100.0);
        (void)health;
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
    std::cout << "Average per update: " << avg_time_us << " μs\n";
    std::cout << "Updates per sec:   " << std::setprecision(0) << updates_per_sec << " Hz\n";
    std::cout << "Time per cell:     " << std::setprecision(3) 
              << avg_time_us / NUM_CELLS << " μs\n";
    std::cout << "============================================================================\n";
    
    // Performance targets for multi-cell
    if (avg_time_us < 1000.0) {
        std::cout << "✓ EXCELLENT: Suitable for real-time BMS (>1kHz)\n";
    } else if (avg_time_us < 10000.0) {
        std::cout << "✓ GOOD: Suitable for production BMS (>100Hz)\n";
    } else {
        std::cout << "⚠  SLOW: May need optimization for real-time use\n";
    }
    
    return 0;
}
