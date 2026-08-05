/*
 * ============================================================================
 * Simple BMS Loop Example
 * ============================================================================
 *
 * Demonstrates basic usage of the DS BMS Middleware in SIMPLE mode
 * (single-cell equivalent pack model).
 *
 * This example is intentionally minimal and deterministic:
 *  - No CAN bus
 *  - No hardware dependencies
 *  - No ML or fleet logic
 *
 * It is designed to:
 *  - Validate integration
 *  - Serve as canonical documentation
 *  - Compile cleanly on any platform
 *
 * ============================================================================
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

#include "ds_bms_middleware_v2.hpp"

using namespace ds_plugin;

int main() {
    std::cout << "=== DS BMS Middleware – Simple Loop Example ===\n";
    std::cout << "Middleware version: " << DSBMSMiddleware::get_version() << "\n\n";

    // ------------------------------------------------------------------------
    // Initialize middleware (single-pack equivalent)
    // ------------------------------------------------------------------------

    DSBMSMiddleware bms;

    // 75 Ah, 400 V nominal EV-style pack
    bms.init(75.0, 400.0);

    // ------------------------------------------------------------------------
    // Simulation parameters
    // ------------------------------------------------------------------------

    constexpr double dt = 0.1;          // 10 Hz update
    constexpr int total_steps = 300;    // 30 seconds runtime

    double soc = 0.80;                  // 80% initial SOC
    double temperature = 30.0;          // °C
    double current = 120.0;             // A (positive = discharge)

    // ------------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------------

    for (int step = 0; step < total_steps; ++step) {

        // Simulate pack voltage sag under load
        double voltage = 400.0 - 0.05 * current;

        // Slowly decrease SOC
        soc -= (current * dt) / (75.0 * 3600.0);
        soc = std::max(0.0, soc);

        // Slight temperature rise
        temperature += 0.005 * std::abs(current);

        // --------------------------------------------------------------------
        // Run DS enhancement cycle
        // --------------------------------------------------------------------

        auto enhanced = bms.enhance_cycle(
            voltage,
            current,
            temperature,
            soc,
            dt
        );

        const auto& diag = bms.diagnostics();

        // --------------------------------------------------------------------
        // Console output (throttled)
        // --------------------------------------------------------------------

        if (step % 10 == 0) {
            std::cout
                << "t=" << diag.time_since_init_s << "s | "
                << "SOC=" << diag.pack_soc_percent << "% | "
                << "SOH=" << diag.pack_soh_percent << "% | "
                << "P=" << diag.instantaneous_power_kw << " kW | "
                << "Trace=" << diag.ds_metric_trace
                << (diag.safety_fault ? " | FAULT" : "")
                << "\n";
        }

        // --------------------------------------------------------------------
        // Safety handling example
        // --------------------------------------------------------------------

        if (!bms.is_safe()) {
            std::cerr << "\nSAFETY FAULT DETECTED:\n";
            for (const auto& fault : bms.get_safety_faults()) {
                std::cerr << "  - " << fault << "\n";
            }
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // ------------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------------

    std::cout << "\n=== Simulation Complete ===\n";
    std::cout << "Total energy discharged: "
              << bms.get_total_energy_out_kwh() << " kWh\n";
    std::cout << "Total energy charged:    "
              << bms.get_total_energy_in_kwh() << " kWh\n";
    std::cout << "Round-trip efficiency:   "
              << bms.get_round_trip_efficiency() * 100.0 << " %\n";

    std::cout << "\nStatus summary:\n";
    std::cout << bms.get_status_summary() << "\n";

    return 0;
}
