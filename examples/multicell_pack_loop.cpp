/*
 * ============================================================================
 * Multi-Cell Pack Example
 * ============================================================================
 *
 * Demonstrates DS BMS Middleware in MULTI_CELL_PACK mode with:
 *  - Per-cell voltage & temperature inputs
 *  - Weak cell detection
 *  - Voltage imbalance monitoring
 *  - Health forecasting
 *
 * This example simulates a 96s EV battery pack.
 *
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <chrono>
#include <algorithm>

#include "ds_bms_middleware_v2.hpp"

using namespace ds_plugin;

int main() {
    std::cout << "=== DS BMS Middleware – Multi-Cell Pack Example ===\n\n";

    // ------------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------------

    MiddlewareConfig config;
    config.mode = MiddlewareConfig::Mode::MULTI_CELL_PACK;
    config.chemistry = ds::advanced::ChemistryType::NMC;
    config.nominal_capacity_ah = 75.0;
    config.nominal_voltage = 400.0;
    config.series_cells = 96;
    config.enable_kalman_filter = true;
    config.enable_cell_balancing = true;
    config.enable_logging = true;

    DSBMSMiddleware bms;
    bms.init_advanced(config);

    // ------------------------------------------------------------------------
    // Simulation parameters
    // ------------------------------------------------------------------------

    constexpr double dt = 0.1;
    constexpr int steps = 400;

    double pack_current = 150.0; // A discharge
    std::vector<double> cell_voltages(config.series_cells, 4.10);
    std::vector<double> cell_temps(config.series_cells, 30.0);

    // Introduce one weak cell
    const int weak_cell_id = 17;

    std::default_random_engine rng(42);
    std::normal_distribution<double> noise(0.0, 0.002);

    // ------------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------------

    for (int i = 0; i < steps; ++i) {

        // Simulate cell behavior
        for (int c = 0; c < config.series_cells; ++c) {
            cell_voltages[c] -= 0.00015 + noise(rng);
            cell_temps[c] += 0.003;
        }

        // Accelerate degradation on one weak cell
        cell_voltages[weak_cell_id] -= 0.001;

        // Update middleware
        bms.update_pack(cell_voltages, cell_temps, pack_current, dt);

        const auto& diag = bms.get_diagnostics();

        // --------------------------------------------------------------------
        // Console output (throttled)
        // --------------------------------------------------------------------

        if (i % 20 == 0) {
            std::cout
                << "t=" << diag.time_since_init_s << "s | "
                << "SOC=" << diag.pack_soc_percent << "% | "
                << "SOH=" << diag.pack_soh_percent << "% | "
                << "ΔV=" << diag.voltage_imbalance_mv << " mV | "
                << "WeakCells=" << diag.weak_cell_count
                << (diag.balancing_required ? " | BALANCE" : "")
                << (diag.safety_fault ? " | FAULT" : "")
                << "\n";
        }

        // --------------------------------------------------------------------
        // Weak cell handling example
        // --------------------------------------------------------------------

        if (diag.weak_cell_warning) {
            std::cout << "\n⚠ Weak cell(s) detected: ";
            for (int id : diag.weak_cell_ids) {
                std::cout << id << " ";
            }
            std::cout << "\n";
        }

        // Safety stop
        if (!bms.is_safe()) {
            std::cerr << "\nSAFETY FAULT:\n";
            for (const auto& fault : bms.get_safety_faults()) {
                std::cerr << "  - " << fault << "\n";
            }
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // ------------------------------------------------------------------------
    // Health forecast
    // ------------------------------------------------------------------------

    auto forecast = bms.get_health_forecast(500.0);

    std::cout << "\n=== Health Forecast ===\n";
    std::cout << "Estimated years to 80% SOH: "
              << forecast.time_to_80_percent_years << "\n";

    std::cout << "\n=== Example Complete ===\n";
    return 0;
}
