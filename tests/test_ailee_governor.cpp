/*
 * ============================================================================
 * UNIT TEST: AILEE HORSEPOWER GOVERNOR & DS TORQUE MANAGER
 * ============================================================================
 *
 * LICENSE: Copyright (c) Don Michael Feeney Jr. Licensed under the MIT License.
 * ============================================================================
 */

#include "ailee_horsepower_governor.hpp"
#include "ds_torque_manager.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void test_hp_calculations() {
    std::cout << "[TEST] Mechanical & Electrical HP Calculations..." << std::endl;
    // Torque 400 Nm at 4000 RPM -> (400 * 4000) / 7121.23 = 224.681 HP
    double hp_mech = AileeHorsepowerGovernor::computeMechanicalHp(400.0, 4000.0);
    assert(std::abs(hp_mech - 224.681) < 0.1);

    // Battery 400V, 400A -> 160 kW -> 160 * 1.34102 = 214.563 HP
    double hp_elec = AileeHorsepowerGovernor::computeElectricalHp(400.0, 400.0);
    assert(std::abs(hp_elec - 214.563) < 0.1);

    double consistency = AileeHorsepowerGovernor::hpConsistencyScore(hp_mech, hp_elec);
    assert(consistency > 0.90);
    std::cout << "  -> PASSED! hp_mech=" << hp_mech << ", hp_elec=" << hp_elec << ", consistency=" << consistency << std::endl;
}

void test_ailee_governor_levels() {
    std::cout << "[TEST] AILEE Horsepower Governor Evaluation..." << std::endl;
    AileeHorsepowerGovernor governor;

    // Normal conditions
    RawSignals normal_sig;
    normal_sig.torque_nm = 300.0;
    normal_sig.rpm = 4000.0;
    normal_sig.v_batt = 380.0;
    normal_sig.i_batt = 350.0;
    normal_sig.ctx.soc = 85.0;
    normal_sig.ctx.soh = 95.0;
    normal_sig.ctx.temp_c = 30.0;
    normal_sig.ctx.sensor_valid = true;

    auto dec_normal = governor.evaluate(normal_sig);
    assert(dec_normal.level == 0);
    assert(dec_normal.trust_score >= 0.85);
    std::cout << "  -> Normal Mode PASSED! Level: " << dec_normal.level << ", Governed HP: " << dec_normal.governed_hp << std::endl;

    // Elevated Temperature -> Level 1 or 2
    RawSignals hot_sig = normal_sig;
    hot_sig.ctx.temp_c = 52.0;
    auto dec_hot = governor.evaluate(hot_sig);
    assert(dec_hot.level >= 1);
    assert(dec_hot.governed_hp < dec_normal.governed_hp);
    std::cout << "  -> Hot Battery Derating PASSED! Level: " << dec_hot.level << ", Governed HP: " << dec_hot.governed_hp << std::endl;

    // Sensor Fault -> Level 3 Protective
    RawSignals fault_sig = normal_sig;
    fault_sig.ctx.sensor_valid = false;
    auto dec_fault = governor.evaluate(fault_sig);
    assert(dec_fault.level == 3);
    assert(dec_fault.used_fallback == true);
    std::cout << "  -> Sensor Fault Protective Mode PASSED! Level: " << dec_fault.level << std::endl;
}

void test_ds_torque_manager() {
    std::cout << "[TEST] DS Torque Manager Integration..." << std::endl;
    ds::drive::DSTorqueManager torque_mgr;

    ds::drive::TorqueCommand cmd;
    cmd.requested_torque_nm = 400.0;
    cmd.motor_rpm = 4000.0;
    cmd.v_batt = 400.0;
    cmd.i_batt = 400.0;
    cmd.ctx.soc = 80.0;
    cmd.ctx.soh = 90.0;
    cmd.ctx.temp_c = 30.0;
    cmd.ctx.sensor_valid = true;

    auto out = torque_mgr.processTorqueCommand(cmd);
    assert(out.applied_torque_nm <= 400.0);
    assert(out.governance_level == 0);
    std::cout << "  -> Normal Torque Command PASSED! Applied Torque: " << out.applied_torque_nm << " Nm" << std::endl;

    // Command under Level 2 Hard Ceiling
    cmd.ctx.temp_c = 55.0; // High temp
    auto out_derated = torque_mgr.processTorqueCommand(cmd);
    assert(out_derated.governance_level >= 2);
    assert(out_derated.applied_torque_nm < out.applied_torque_nm);
    assert(out_derated.derating_active == true);
    std::cout << "  -> Derated Torque Command PASSED! Governed Torque: " << out_derated.applied_torque_nm << " Nm" << std::endl;
}

int main() {
    std::cout << "===========================================" << std::endl;
    std::cout << "RUNNING AILEE GOVERNOR & TORQUE MANAGER TESTS" << std::endl;
    std::cout << "===========================================" << std::endl;

    try {
        test_hp_calculations();
        test_ailee_governor_levels();
        test_ds_torque_manager();
        std::cout << "===========================================" << std::endl;
        std::cout << "ALL C++ TESTS PASSED SUCCESSFULLY!" << std::endl;
        std::cout << "===========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
