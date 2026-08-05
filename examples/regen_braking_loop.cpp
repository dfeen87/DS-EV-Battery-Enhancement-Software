#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

#include "ds_bms_middleware_v2.hpp"
#include "ds_regen_braking_manager_v1.hpp"

int main() {
    std::cout << "=== DS Regen Braking Manager Example ===\n";
    std::cout << "Regen module version: " << ds::drive::DSRegenBrakingManager::get_version() << "\n\n";

    // Initialize BMS middleware (simple mode)
    ds_plugin::DSBMSMiddleware bms;
    bms.init(75.0, 400.0);

    // Regen manager
    ds::drive::RegenConfig rcfg;
    rcfg.peak_regen_torque_nm = 220.0;
    rcfg.max_regen_power_kw = 120.0;

    ds::drive::DSRegenBrakingManager regen(rcfg);

    constexpr double dt = 0.01;   // 100 Hz control loop
    double t = 0.0;

    // Simple simulated conditions
    double soc = 0.92;            // high SOC to show regen taper
    double pack_voltage = 410.0;
    double pack_current = -50.0;  // charging (regen would be negative current in real system)
    double pack_temp = 20.0;

    double vehicle_speed_kph = 60.0;
    double motor_rpm = 5000.0;

    for (int i = 0; i < 1200; ++i) { // ~12 seconds

        // Simulate a braking event: ramp brake request up then down
        double brake_request = 0.0;
        if (t > 1.0 && t < 5.0) {
            brake_request = std::min(1.0, (t - 1.0) / 1.0); // ramp to 1.0
        } else if (t >= 5.0 && t < 7.0) {
            brake_request = std::max(0.0, 1.0 - (t - 5.0) / 2.0); // ramp down
        }

        // Simulate ABS event briefly
        bool abs_active = (t > 3.2 && t < 3.6);
        bool wheel_slip = false;

        // Update BMS enhanced state (simple; in real systems you use real measurements)
        auto enhanced = bms.enhance_cycle(pack_voltage, pack_current, pack_temp, soc, dt);
        const auto& diag = bms.get_diagnostics();

        // Compute regen limit + blend recommendation
        auto regen_result = regen.compute_regen_limit(
            enhanced,
            &diag,
            brake_request,
            vehicle_speed_kph,
            motor_rpm,
            abs_active,
            wheel_slip,
            dt
        );

        if (i % 50 == 0) {
            std::cout
                << "t=" << t << "s | brake=" << brake_request
                << " | regen_max=" << regen_result.max_regen_torque_nm << "Nm"
                << " | frac=" << regen_result.regen_fraction
                << " | limit=" << regen_result.limiting_factor
                << (abs_active ? " | ABS" : "")
                << "\n";
        }

        // Slowly change SOC/voltage to demonstrate taper behavior
        soc = std::min(0.99, soc + 0.00002); // SOC creeping upward during “regen”
        pack_voltage = std::min(448.0, pack_voltage + 0.002); // voltage creeping up

        t += dt;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n=== Done ===\n";
    return 0;
}
