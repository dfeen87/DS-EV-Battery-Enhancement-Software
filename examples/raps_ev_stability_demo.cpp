/*
 * ============================================================================
 * RAPS EV STABILITY MEMBRANE DEMONSTRATION
 * ============================================================================
 */

#include "raps_ev_stability_membrane.hpp"
#include "torque_enhancement.hpp"
#include "ds_torque_manager.hpp"
#include "ds_regen_braking_manager_v1.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=====================================================\n";
    std::cout << " RAPS EV STABILITY MEMBRANE DEMONSTRATION & BENCHMARK\n";
    std::cout << "=====================================================\n\n";

    // 1. Instantiating RAPS EV Stability Membrane
    raps::ev::StabilityConfig cfg;
    raps::ev::RapsEVStabilityMembrane membrane(cfg);

    std::cout << "1. Simulating Voltage Sag Scenario:\n";
    std::cout << "-----------------------------------------------------\n";
    double voltages[] = { 390.0, 360.0, 335.0, 325.0, 315.0 };
    for (double v : voltages) {
        auto st = membrane.evaluate(v, 150.0, 30.0);
        std::cout << "  Pack Voltage: " << v << "V | Sag Ratio: "
                  << std::fixed << std::setprecision(3) << st.voltage_sag_ratio
                  << " | Overall Stability: " << st.overall_membrane_stability
                  << " | DSM Tripped: " << (st.dsm_tripped ? std::string("YES (") + st.get_dsm_trip_reason() + ")" : "NO")
                  << "\n";
    }

    std::cout << "\n2. Simulating Current Spikes and Regen Surge Damping:\n";
    std::cout << "-----------------------------------------------------\n";
    membrane.init(cfg);
    // Baseline
    membrane.evaluate(400.0, 50.0, 25.0, nullptr, 0.01);

    // Current Spike 50A -> 450A
    auto st_spike = membrane.evaluate(400.0, 450.0, 25.0, nullptr, 0.01);
    std::cout << "  Current Spike (450A): Spike Ratio = " << st_spike.current_spike_ratio
              << " | Overall Stability = " << st_spike.overall_membrane_stability << "\n";

    // Regen Surge (-250A in 10ms)
    auto st_surge = membrane.evaluate(400.0, -250.0, 25.0, nullptr, 0.01);
    std::cout << "  Regen Surge (-250A): Surge Ratio = " << st_surge.regen_surge_ratio
              << " | Overall Stability = " << st_surge.overall_membrane_stability << "\n";

    std::cout << "\n3. Integrated Torque Manager + AILEE Governance Boost:\n";
    std::cout << "-----------------------------------------------------\n";
    ds::drive::DSAileeTorqueManager ailee_tm;
    ds::drive::TorqueCommand cmd;
    cmd.requested_torque_nm = 350.0;
    cmd.motor_rpm = 4500.0;
    cmd.v_batt = 400.0;
    cmd.i_batt = 120.0;
    cmd.ctx.soc = 85.0;
    cmd.ctx.soh = 95.0;
    cmd.ctx.temp_c = 28.0;
    cmd.ctx.sensor_valid = true;

    auto gov_out = ailee_tm.processTorqueCommand(cmd);
    std::cout << "  Governance Level: " << gov_out.governance_level << "\n"
              << "  Applied Torque: " << gov_out.applied_torque_nm << " Nm\n"
              << "  Applied Horsepower: " << gov_out.applied_hp << " HP\n"
              << "  RAPS Membrane Stability: " << gov_out.raps_membrane_stability << "\n"
              << "  RAPS Boost Multiplier: " << gov_out.raps_boost_multiplier << "x\n"
              << "  Governance Reason: " << gov_out.reason << "\n";

    std::cout << "\n=====================================================\n";
    std::cout << " RAPS EV Stability Demonstration Complete.\n";
    std::cout << "=====================================================\n";

    return 0;
}
