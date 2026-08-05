/*
 * ============================================================================
 * DS BATTERY ENHANCEMENT - MULTI-CELL PACK EXAMPLE
 * ============================================================================
 * 
 * Demonstrates multi-cell pack management with per-cell tracking,
 * weak cell detection, and cell balancing recommendations.
 * 
 * This is what real EV automakers need for production vehicles.
 * 
 * Compile:
 *   g++ -std=c++17 -O3 -I../include examples/multi_cell_pack.cpp -o multicell_demo
 * 
 * ============================================================================
 */

#include "ds_battery_enhancement.hpp"
#include "ds_bms_middleware_v2.hpp"
#include "ds_advanced_features.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cmath>

// ============================================================================
// SIMULATED MULTI-CELL PACK SENSORS
// ============================================================================

class MultiCellSimulator {
private:
    int num_cells_;
    std::vector<double> cell_capacities_;     // Relative capacities
    std::vector<double> cell_resistances_;    // Internal resistances
    std::vector<double> cell_temperatures_;   // Individual temps
    std::mt19937 rng_;
    
public:
    MultiCellSimulator(int num_cells, unsigned seed = 42) 
        : num_cells_(num_cells), rng_(seed) {
        
        // Initialize with slight cell-to-cell variance
        std::normal_distribution<double> cap_dist(1.0, 0.02);  // 2% variance
        std::normal_distribution<double> res_dist(0.01, 0.001); // 0.01Ω ±0.001Ω
        
        cell_capacities_.resize(num_cells_);
        cell_resistances_.resize(num_cells_);
        cell_temperatures_.resize(num_cells_);
        
        for (int i = 0; i < num_cells_; ++i) {
            cell_capacities_[i] = cap_dist(rng_);
            cell_resistances_[i] = res_dist(rng_);
            cell_temperatures_[i] = 25.0;
        }
        
        // Make a few cells deliberately weaker (simulate manufacturing defects)
        if (num_cells_ > 5) {
            cell_capacities_[5] = 0.92;   // Weak cell
            cell_resistances_[5] = 0.015;
        }
        if (num_cells_ > 27) {
            cell_capacities_[27] = 0.90;  // Weaker cell
            cell_resistances_[27] = 0.018;
        }
    }
    
    std::vector<double> get_cell_voltages(double avg_soc, double pack_current) {
        std::vector<double> voltages(num_cells_);
        
        for (int i = 0; i < num_cells_; ++i) {
            // Base voltage from OCV curve (simplified)
            double base_v = 3.2 + 0.8 * avg_soc;
            
            // Voltage drop due to internal resistance
            double v_drop = pack_current * cell_resistances_[i];
            
            // Cell-to-cell variance
            double variance = (cell_capacities_[i] - 1.0) * 0.1;
            
            voltages[i] = base_v - v_drop + variance;
        }
        
        return voltages;
    }
    
    std::vector<double> get_cell_temperatures(double pack_current, double ambient) {
        // Simple thermal model
        for (int i = 0; i < num_cells_; ++i) {
            // Heat from I²R losses
            double heat = pack_current * pack_current * cell_resistances_[i];
            
            // Temperature rise (simplified)
            cell_temperatures_[i] = ambient + heat * 100.0;
            
            // Boundary cells are cooler
            if (i < 3 || i >= num_cells_ - 3) {
                cell_temperatures_[i] -= 3.0;
            }
        }
        
        return cell_temperatures_;
    }
    
    void age_cells(int cycles) {
        // Degrade cells based on their weakness
        for (int i = 0; i < num_cells_; ++i) {
            double degradation = cycles * 0.00005; // 0.005% per cycle
            
            // Weak cells degrade faster
            if (cell_capacities_[i] < 0.95) {
                degradation *= 1.5;
            }
            
            cell_capacities_[i] *= (1.0 - degradation);
            cell_resistances_[i] *= (1.0 + degradation);
        }
    }
};

// ============================================================================
// EXAMPLE 1: BASIC MULTI-CELL MONITORING
// ============================================================================

void example_1_basic_monitoring() {
    std::cout << "\n=== EXAMPLE 1: Basic Multi-Cell Monitoring ===\n\n";
    
    const int NUM_CELLS = 96;  // Tesla-style pack
    
    // Configure for multi-cell pack
    ds_plugin::MiddlewareConfig config;
    config.chemistry = ds::advanced::ChemistryType::NMC;
    config.nominal_capacity_ah = 75.0;
    config.nominal_voltage = 400.0;
    config.series_cells = NUM_CELLS;
    config.mode = ds_plugin::MiddlewareConfig::Mode::MULTI_CELL_PACK;
    config.enable_kalman_filter = true;
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init_advanced(config);
    
    // Create simulator
    MultiCellSimulator sim(NUM_CELLS);
    
    std::cout << "Monitoring " << NUM_CELLS << "-cell NMC pack...\n\n";
    
    // Simulate 10 cycles
    for (int cycle = 0; cycle < 10; ++cycle) {
        for (int step = 0; step < 100; ++step) {
            double time = (cycle + step / 100.0);
            double soc = 0.5 + 0.4 * std::sin(time * 6.28);
            double current = (step < 50) ? 50.0 : -75.0;
            
            auto voltages = sim.get_cell_voltages(soc, current);
            auto temps = sim.get_cell_temperatures(current, 25.0);
            
            middleware.update_pack(voltages, temps, current, 0.1);
        }
        
        // Age cells
        sim.age_cells(1);
        
        // Check status
        auto diag = middleware.get_diagnostics();
        std::cout << "Cycle " << cycle << ": ";
        std::cout << "Health=" << std::fixed << std::setprecision(1) 
                  << diag.pack_health_percent << "%, ";
        std::cout << "Weak cells=" << diag.weak_cell_ids.size() << "\n";
    }
}

// ============================================================================
// EXAMPLE 2: WEAK CELL DETECTION
// ============================================================================

void example_2_weak_cell_detection() {
    std::cout << "\n=== EXAMPLE 2: Weak Cell Detection ===\n\n";
    
    const int NUM_CELLS = 96;
    
    ds_plugin::MiddlewareConfig config;
    config.chemistry = ds::advanced::ChemistryType::NMC;
    config.nominal_capacity_ah = 75.0;
    config.nominal_voltage = 400.0;
    config.series_cells = NUM_CELLS;
    config.mode = ds_plugin::MiddlewareConfig::Mode::MULTI_CELL_PACK;
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init_advanced(config);
    
    MultiCellSimulator sim(NUM_CELLS);
    
    // Age the pack significantly
    std::cout << "Aging pack through 200 cycles...\n";
    
    for (int cycle = 0; cycle < 200; ++cycle) {
        for (int step = 0; step < 50; ++step) {
            double time = (cycle + step / 50.0);
            double soc = 0.5 + 0.4 * std::sin(time * 6.28);
            double current = (step < 25) ? 50.0 : -50.0;
            
            auto voltages = sim.get_cell_voltages(soc, current);
            auto temps = sim.get_cell_temperatures(current, 25.0);
            
            middleware.update_pack(voltages, temps, current, 0.1);
        }
        
        sim.age_cells(1);
    }
    
    // Check for weak cells
    auto weak_cells = middleware.get_weak_cells();
    
    std::cout << "\n📊 Weak Cell Report:\n";
    std::cout << "Total weak cells detected: " << weak_cells.size() << "\n";
    
    if (!weak_cells.empty()) {
        std::cout << "Weak cell IDs: ";
        for (int id : weak_cells) {
            std::cout << id << " ";
        }
        std::cout << "\n\n";
        std::cout << "⚠️  Recommendation: Schedule maintenance\n";
        std::cout << "   Consider cell replacement for: ";
        for (int id : weak_cells) {
            std::cout << "Cell #" << id << " ";
        }
        std::cout << "\n";
    }
    
    auto health = middleware.get_health_forecast(500.0);
    std::cout << "\nPack health forecast:\n";
    std::cout << "  Current capacity: " << health.remaining_capacity_percent << "%\n";
    std::cout << "  Cycles to 80%: " << (int)health.cycles_to_80_percent << "\n";
}

// ============================================================================
// EXAMPLE 3: CELL IMBALANCE MONITORING
// ============================================================================

void example_3_cell_imbalance() {
    std::cout << "\n=== EXAMPLE 3: Cell Imbalance Monitoring ===\n\n";
    
    const int NUM_CELLS = 96;
    
    ds_plugin::MiddlewareConfig config;
    config.chemistry = ds::advanced::ChemistryType::NMC;
    config.nominal_capacity_ah = 75.0;
    config.nominal_voltage = 400.0;
    config.series_cells = NUM_CELLS;
    config.mode = ds_plugin::MiddlewareConfig::Mode::MULTI_CELL_PACK;
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init_advanced(config);
    
    MultiCellSimulator sim(NUM_CELLS);
    
    std::cout << "Monitoring voltage imbalance over 50 cycles...\n\n";
    std::cout << std::setw(8) << "Cycle"
              << std::setw(15) << "Imbalance(mV)"
              << std::setw(12) << "Status\n";
    std::cout << std::string(35, '-') << "\n";
    
    for (int cycle = 0; cycle < 50; ++cycle) {
        // Run one cycle
        for (int step = 0; step < 100; ++step) {
            double time = (cycle + step / 100.0);
            double soc = 0.5 + 0.4 * std::sin(time * 6.28);
            double current = (step < 50) ? 50.0 : -75.0;
            
            auto voltages = sim.get_cell_voltages(soc, current);
            auto temps = sim.get_cell_temperatures(current, 25.0);
            
            middleware.update_pack(voltages, temps, current, 0.1);
        }
        
        sim.age_cells(1);
        
        // Print every 5 cycles
        if (cycle % 5 == 0) {
            auto diag = middleware.get_diagnostics();
            std::cout << std::setw(8) << cycle
                      << std::setw(15) << std::fixed << std::setprecision(1)
                      << diag.voltage_imbalance_mv;
            
            if (diag.imbalance_warning) {
                std::cout << std::setw(12) << "⚠️  WARNING";
            } else {
                std::cout << std::setw(12) << "✓ OK";
            }
            std::cout << "\n";
        }
    }
    
    std::cout << "\nNote: Imbalance >100mV triggers cell balancing recommendation\n";
}

// ============================================================================
// EXAMPLE 4: CHEMISTRY COMPARISON (Multi-Cell)
// ============================================================================

void example_4_chemistry_comparison() {
    std::cout << "\n=== EXAMPLE 4: Chemistry Comparison (Multi-Cell) ===\n\n";
    
    std::vector<ds::advanced::ChemistryType> chemistries = {
        ds::advanced::ChemistryType::LFP,
        ds::advanced::ChemistryType::NMC,
        ds::advanced::ChemistryType::NCA
    };
    
    std::vector<std::string> names = {"LFP", "NMC", "NCA"};
    std::vector<int> cell_counts = {120, 96, 96};  // Different cell counts
    
    std::cout << std::setw(8) << "Chem"
              << std::setw(10) << "Cells"
              << std::setw(15) << "Health@100cyc"
              << std::setw(15) << "Cycles to 80%\n";
    std::cout << std::string(48, '-') << "\n";
    
    for (size_t i = 0; i < chemistries.size(); ++i) {
        ds_plugin::MiddlewareConfig config;
        config.chemistry = chemistries[i];
        config.nominal_capacity_ah = 75.0;
        config.nominal_voltage = 400.0;
        config.series_cells = cell_counts[i];
        config.mode = ds_plugin::MiddlewareConfig::Mode::MULTI_CELL_PACK;
        
        ds_plugin::DSBMSMiddleware middleware;
        middleware.init_advanced(config);
        
        MultiCellSimulator sim(cell_counts[i]);
        
        // Run 100 cycles
        for (int cycle = 0; cycle < 100; ++cycle) {
            for (int step = 0; step < 50; ++step) {
                double time = (cycle + step / 50.0);
                double soc = 0.5 + 0.4 * std::sin(time * 6.28);
                double current = (step < 25) ? 50.0 : -50.0;
                
                auto voltages = sim.get_cell_voltages(soc, current);
                auto temps = sim.get_cell_temperatures(current, 25.0);
                
                middleware.update_pack(voltages, temps, current, 0.1);
            }
            sim.age_cells(1);
        }
        
        auto health = middleware.get_health_forecast(1000.0);
        
        std::cout << std::setw(8) << names[i]
                  << std::setw(10) << cell_counts[i]
                  << std::setw(15) << std::fixed << std::setprecision(1)
                  << health.remaining_capacity_percent << "%"
                  << std::setw(15) << (int)health.cycles_to_80_percent << "\n";
    }
    
    std::cout << "\nKey Insights:\n";
    std::cout << "  - LFP: Longest life, more cells needed (lower voltage)\n";
    std::cout << "  - NMC: Good balance of energy and life\n";
    std::cout << "  - NCA: Highest energy density, needs careful management\n";
}

// ============================================================================
// EXAMPLE 5: THERMAL MANAGEMENT
// ============================================================================

void example_5_thermal_management() {
    std::cout << "\n=== EXAMPLE 5: Thermal Management ===\n\n";
    
    const int NUM_CELLS = 96;
    
    ds_plugin::MiddlewareConfig config;
    config.chemistry = ds::advanced::ChemistryType::NMC;
    config.nominal_capacity_ah = 75.0;
    config.nominal_voltage = 400.0;
    config.series_cells = NUM_CELLS;
    config.mode = ds_plugin::MiddlewareConfig::Mode::MULTI_CELL_PACK;
    
    ds_plugin::DSBMSMiddleware middleware;
    middleware.init_advanced(config);
    
    MultiCellSimulator sim(NUM_CELLS);
    
    std::cout << "Monitoring thermal behavior during fast charging...\n\n";
    std::cout << std::setw(8) << "Time(s)"
              << std::setw(12) << "Current(A)"
              << std::setw(12) << "Avg Temp"
              << std::setw(15) << "Max Temp"
              << std::setw(12) << "Status\n";
    std::cout << std::string(59, '-') << "\n";
    
    // Fast charging simulation
    for (int step = 0; step < 100; ++step) {
        double time = step * 10.0; // 10 second intervals
        double current = 150.0; // Fast charging at 150A (2C rate)
        double soc = std::min(0.95, step / 100.0);
        
        auto voltages = sim.get_cell_voltages(soc, current);
        auto temps = sim.get_cell_temperatures(current, 25.0);
        
        middleware.update_pack(voltages, temps, current, 0.1);
        
        // Calculate temperature statistics
        double avg_temp = 0.0;
        double max_temp = temps[0];
        for (double t : temps) {
            avg_temp += t;
            max_temp = std::max(max_temp, t);
        }
        avg_temp /= temps.size();
        
        if (step % 10 == 0) {
            std::cout << std::setw(8) << (int)time
                      << std::setw(12) << current
                      << std::setw(12) << std::fixed << std::setprecision(1)
                      << avg_temp
                      << std::setw(15) << max_temp;
            
            if (max_temp > 45.0) {
                std::cout << std::setw(12) << "⚠️  HOT";
            } else {
                std::cout << std::setw(12) << "✓ OK";
            }
            std::cout << "\n";
        }
    }
    
    std::cout << "\n💡 Tip: DS tracks thermal entropy to predict thermal runaway risk\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "============================================================================\n";
    std::cout << "DS MULTI-CELL PACK MANAGEMENT EXAMPLES\n";
    std::cout << "============================================================================\n";
    
    try {
        example_1_basic_monitoring();
        example_2_weak_cell_detection();
        example_3_cell_imbalance();
        example_4_chemistry_comparison();
        example_5_thermal_management();
        
        std::cout << "\n============================================================================\n";
        std::cout << "All multi-cell examples completed successfully!\n";
        std::cout << "============================================================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
