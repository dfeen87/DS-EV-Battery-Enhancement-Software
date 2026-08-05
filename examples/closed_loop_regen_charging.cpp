/*
 * ============================================================================
 * Closed-Loop Regen Charging Example
 * ============================================================================
 *
 * Demonstrates a full EV energy control loop:
 *
 *   Battery → Torque → Braking → Battery
 *
 * - Regen braking limits are computed from battery state
 * - Regen torque produces electrical charging current
 * - Charging current feeds back into the BMS
 * - Battery state updates influence the *next* braking decision
 *
 * This example uses MEASURED-CURRENT semantics conceptually,
 * but simulates the current for demonstration purposes.
 *
 * ============================================================================
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

#include "ds_bms_middleware_v2.hpp"
#include "ds_regen_braking_manager_v1.hpp"
#include "ds_energy_recovery_coordinator_v1.hpp"

int main() {
    std::cout << "\n=== DS Closed-Loop Regen Charging Demo ===\n\n";

    // ------------------------------------------------------------------------
    // Initialize BMS (simple mode)
    // ------------------------------------------------------------------------
    ds_plugin::DSBMSMiddleware bms;
    bms.init(75.0, 400.0);  // 75Ah, 400V pack

    // ------------------------------------------------------------------------
    // Regen braking manager
    // ------------------------------------------------------------------------
    ds::drive::RegenConfig regen_cfg;
    regen_cfg.peak_regen_torque_nm = 220.0;
    regen_cfg.max_regen_power_kw = 120.0;

    ds::drive::DSRegenBrakingManager regen_mgr(regen_cfg);

    // ------------------------------------------------------------------------
    // Energy recovery coordinator
    // ------------------------------------------------------------------------
    ds::drive::EnergyRecoveryConfig rec_cfg;
    rec_cfg.prefer_measured_pack_current = false; // simulate estimated current
    rec_cfg.drivetrain_efficiency_regen = 0.88;

    ds::drive::DSEnergyRecoveryCoordinator recovery(rec_cfg);

    // ------------------------------------------------------------------------
    // Simulated vehicle / battery state
    // ------------------------------------------------------------------------
    constexpr double dt = 0.01;   // 100 Hz loop
    double t = 0.0;

    double soc = 0.88;
    double pack_voltage = 405.0;
    double pack_temp = 25.0;

    double vehicle_speed_kph = 90.0;
    double motor_omega_rad_s = 520.0; // ~5000 rpm equivalent

    double pack_current_measured = 0.0;

    // ------------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------------
    for (int step = 0; step < 2000; ++step) {

        // Simulated brake pedal input
        double brake_request = 0.0;
        if (t > 1.0 && t < 6.0) {
            brake_request = std::min(1.0, (t - 1.0) / 1.5);
        } else if (t >= 6.0 && t < 8.0) {
            brake_request = std::max(0.0, 1.0 - (t - 6.0) / 2.0);
        }

        // Simulated ABS event (brief)
        bool abs_active = (t > 3.4 && t < 3.7);
        bool wheel_slip = false;

        // --------------------------------------------------------------------
        // 1) Regen braking decision (battery-aware)
        // --------------------------------------------------------------------
        auto enhanced_prev = bms.get_enhanced_state();
        auto diag_prev = bms.get_diagnostics();

        auto regen = regen_mgr.compute_regen_limit(
            enhanced_prev,
            &diag_prev,
            brake_request,
            vehicle_speed_kph,
            motor_omega_rad_s,
            abs_active,
            wheel_slip,
            dt
        );

        // --------------------------------------------------------------------
        // 2) Energy recovery → battery charging update
        // --------------------------------------------------------------------
        auto result = recovery.step(
            bms,
            regen,
            pack_voltage,
            pack_temp,
            pack_current_measured, // unused in estimated mode
            soc,
            motor_omega_rad_s,
            dt
        );

        // --------------------------------------------------------------------
        // 3) Update simulated pack state from charging
        // --------------------------------------------------------------------
        pack_current_measured = result.pack_current_a_used;

        // SOC rises during regen
        soc = std::min(0.99, soc + std::abs(pack_current_measured) * dt / 3600.0 / 75.0);

        // Voltage rises slowly as SOC increases
        pack_voltage = std::min(448.0, pack_voltage + 0.01 * regen.regen_fraction);

        // Vehicle slows down
        vehicle_speed_kph = std::max(0.0, vehicle_speed_kph - brake_request * 0.6);
        motor_omega_rad_s = std::max(0.0, motor_omega_rad_s - brake_request * 4.0);

        // --------------------------------------------------------------------
        // Console output (throttled)
        // --------------------------------------------------------------------
        if (step % 50 == 0) {
            const auto& d = result.bms_diag;

            std::cout
                << "t=" << t << "s"
                << " | brake=" << brake_request
                << " | regenNm=" << regen.max_regen_torque_nm
                << " | frac=" << regen.regen_fraction
                << " | I=" << pack_current_measured << "A"
                << " | SOC=" << d.pack_soc_percent << "%"
                << " | limit=" << regen.limiting_factor
                << (abs_active ? " | ABS" : "")
                << "\n";
        }

        // --------------------------------------------------------------------
        t += dt;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ------------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------------
    const auto& rec_diag = recovery.diagnostics();

    std::cout << "\n=== Energy Recovery Summary ===\n";
    std::cout << "Recovered energy (session): "
              << rec_diag.recovered_energy_kwh_session << " kWh\n";
    std::cout << "Recovered energy (total):   "
              << rec_diag.recovered_energy_kwh_total << " kWh\n";

    std::cout << "\n=== Demo Complete ===\n";
    return 0;
}
