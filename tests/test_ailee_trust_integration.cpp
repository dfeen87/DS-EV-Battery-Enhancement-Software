/*
 * ============================================================================
 * AILEE TRUST LAYER INTEGRATION TEST SUITE
 * ============================================================================
 *
 * Tests trust-gated continuous learning adaptation, Level 3 Protective Mode fallback,
 * parameter snapshotting, rollbacks, and backward-compatibility adapter shims.
 *
 * License: MIT License. Copyright (c) Don Michael Feeney Jr.
 * ============================================================================
 */

#include "ailee_trust_layer/ailee_compartment.hpp"
#include "ailee_trust_layer/ailee_trust_gate.hpp"
#include "ailee_trust_layer/bms_compartment.hpp"
#include "ailee_trust_layer/anomaly_compartment.hpp"
#include "ailee_trust_layer/learning_engine.hpp"
#include "ailee_trust_layer/ailee_adapters.hpp"

#include <cassert>
#include <iostream>
#include <cmath>
#include <limits>

void test_trust_gated_learning() {
    std::cout << "[TEST] Running test_trust_gated_learning...\n";
    ailee::ev::TrustGate trust_gate;
    ailee::ev::LearningEngine engine("logs/test_ailee_learning_audit.json", "logs/test_ailee_automotive_audit.log");

    ailee::ev::ParameterEnvelope envelope;
    envelope.param_name = "fast_charge_c_rate";
    envelope.min_value = 0.5;
    envelope.max_value = 3.0;
    envelope.max_step_delta = 0.10;
    envelope.current_value = 1.0;
    envelope.default_value = 1.0;

    // Case A: High trust score (0.95), valid proposed value (1.05) -> SHOULD APPROVE
    ailee::ev::ParameterSnapshot snap1;
    bool res1 = engine.propose_parameter_update("ChargingCompartment", envelope, 1.05, 0.95, trust_gate, &snap1);
    assert(res1 == true);
    assert(envelope.current_value == 1.05);
    assert(snap1.approved == true);
    (void)res1;

    // Case B: Low trust score (0.75 < 0.85), valid proposed step -> SHOULD REJECT
    ailee::ev::ParameterSnapshot snap2;
    bool res2 = engine.propose_parameter_update("ChargingCompartment", envelope, 1.10, 0.75, trust_gate, &snap2);
    assert(res2 == false);
    assert(envelope.current_value == 1.05); // Unchanged
    assert(snap2.approved == false);
    (void)res2;

    // Case C: High trust score (0.90), but proposed step too large (1.50 vs max_step_delta=0.10) -> SHOULD REJECT
    ailee::ev::ParameterSnapshot snap3;
    bool res3 = engine.propose_parameter_update("ChargingCompartment", envelope, 1.50, 0.90, trust_gate, &snap3);
    assert(res3 == false);
    assert(envelope.current_value == 1.05); // Unchanged
    assert(snap3.approved == false);
    (void)res3;

    // Verify alias field synchronization in snapshot struct
    assert(snap1.rollback_handle == snap1.snapshot_id);
    assert(snap1.compartment_id == "ChargingCompartment");
    assert(snap1.previous_params == 1.0);
    assert(snap1.proposed_params == 1.05);
    assert(snap1.applied_params == 1.05);

    std::cout << "  -> test_trust_gated_learning PASSED.\n";
}

void test_protective_mode_exit_recovery() {
    std::cout << "[TEST] Running test_protective_mode_exit_recovery...\n";
    ailee::ev::TrustGate trust_gate;

    // Recovery fails if active anomaly is present
    bool exit1 = trust_gate.verify_protective_mode_exit(0.95, true, 100);
    assert(exit1 == false);
    (void)exit1;

    // Recovery fails if trust score < 0.85
    bool exit2 = trust_gate.verify_protective_mode_exit(0.80, false, 100);
    assert(exit2 == false);
    (void)exit2;

    // Recovery fails if consecutive healthy cycles < hysteresis threshold (50)
    bool exit3 = trust_gate.verify_protective_mode_exit(0.90, false, 30);
    assert(exit3 == false);
    (void)exit3;

    // Recovery succeeds when trust_score >= 0.85, no anomaly, and cycles >= 50
    bool exit4 = trust_gate.verify_protective_mode_exit(0.92, false, 55);
    assert(exit4 == true);
    (void)exit4;

    std::cout << "  -> test_protective_mode_exit_recovery PASSED.\n";
}

void test_protective_mode_fallback() {
    std::cout << "[TEST] Running test_protective_mode_fallback...\n";
    ailee::ev::TrustGate trust_gate;
    ailee::ev::BMSCompartment bms;
    ailee::ev::AnomalyCompartment anomaly;

    bms.initialize();
    anomaly.initialize();

    // Normal evaluation
    ailee::ev::BMSInputData normal_bms;
    bms.update_inputs(normal_bms);
    auto t_bms1 = bms.evaluate(0.01);

    ailee::ev::AnomalyInputData normal_anom;
    normal_anom.torque_nm = 300.0;
    normal_anom.rpm = 3000.0;
    normal_anom.v_batt = 400.0;
    normal_anom.i_batt = 235.6; // HP mech and electrical HP both ~126.4
    anomaly.update_inputs(normal_anom);
    auto t_anom1 = anomaly.evaluate(0.01);

    ailee::ev::CompartmentTelemetry telems1[] = {t_bms1, t_anom1};
    auto dec1 = trust_gate.evaluate_telemetry(telems1, 2);
    assert(dec1.learning_allowed == true);
    (void)dec1;

    // Inject Critical Anomaly: Sensor failure or extreme temp
    ailee::ev::BMSInputData fault_bms = normal_bms;
    fault_bms.temperature_c = 68.0; // Over 60C critical threshold
    bms.update_inputs(fault_bms);
    auto t_bms2 = bms.evaluate(0.01);

    ailee::ev::CompartmentTelemetry telems2[] = {t_bms2, t_anom1};
    auto dec2 = trust_gate.evaluate_telemetry(telems2, 2);

    assert(dec2.level == ailee::ev::GovernanceLevel::LEVEL_3_PROTECTIVE);
    assert(dec2.derate_factor == 0.25);
    assert(dec2.learning_allowed == false);
    assert(dec2.fallback_active == true);
    (void)dec2;

    std::cout << "  -> test_protective_mode_fallback PASSED.\n";
}

void test_rollback_mechanism() {
    std::cout << "[TEST] Running test_rollback_mechanism...\n";
    ailee::ev::TrustGate trust_gate;
    ailee::ev::LearningEngine engine("logs/test_ailee_learning_audit.json", "logs/test_ailee_automotive_audit.log");

    ailee::ev::ParameterEnvelope envelope;
    envelope.param_name = "regen_torque_limit";
    envelope.min_value = 50.0;
    envelope.max_value = 300.0;
    envelope.max_step_delta = 20.0;
    envelope.current_value = 100.0;

    ailee::ev::ParameterSnapshot snap;
    bool approved = engine.propose_parameter_update("BMSCompartment", envelope, 115.0, 0.92, trust_gate, &snap);
    assert(approved == true);
    assert(envelope.current_value == 115.0);
    (void)approved;

    // Rollback using rollback_handle alias
    bool rb_res = engine.rollback(snap.rollback_handle, envelope);
    assert(rb_res == true);
    assert(envelope.current_value == 100.0);
    (void)rb_res;

    std::cout << "  -> test_rollback_mechanism PASSED.\n";
}

void test_adapter_shims() {
    std::cout << "[TEST] Running test_adapter_shims...\n";
    ailee::ev::adapters::HorsepowerGovernorAdapter hp_adapter;
    hp_adapter.initialize();

    RawSignals sigs;
    sigs.torque_nm = 300.0;
    sigs.rpm = 3000.0;
    sigs.v_batt = 400.0;
    sigs.i_batt = 300.0;
    sigs.ctx.soc = 85.0;
    sigs.ctx.soh = 95.0;
    sigs.ctx.temp_c = 25.0;
    sigs.ctx.sensor_valid = true;

    hp_adapter.set_raw_signals(sigs);
    auto t = hp_adapter.evaluate(0.01);
    assert(t.trust_score >= 0.80);
    (void)t;

    ailee::ev::adapters::BMSMiddlewareAdapter bms_adapter;
    bms_adapter.initialize();
    bms_adapter.update_cycle(400.0, 10.0, 25.0, 80.0, 0.01);
    auto t_bms = bms_adapter.evaluate(0.01);
    assert(t_bms.health_score > 0.0);
    (void)t_bms;

    ailee::ev::adapters::TorqueManagerAdapter tm_adapter;
    tm_adapter.initialize();
    ds::drive::TorqueCommand cmd;
    cmd.requested_torque_nm = 200.0;
    cmd.motor_rpm = 2500.0;
    cmd.v_batt = 400.0;
    cmd.i_batt = 150.0;
    cmd.ctx.soc = 80.0;
    cmd.ctx.soh = 90.0;
    cmd.ctx.temp_c = 28.0;
    cmd.ctx.sensor_valid = true;
    tm_adapter.set_torque_command(cmd);
    auto t_tm = tm_adapter.evaluate(0.01);
    assert(t_tm.trust_score >= 0.70);
    (void)t_tm;

    std::cout << "  -> test_adapter_shims PASSED.\n";
}

void test_malformed_evidence_fails_closed() {
    std::cout << "[TEST] Running test_malformed_evidence_fails_closed...\n";
    ailee::ev::TrustGate gate;
    ailee::ev::CompartmentTelemetry evidence;
    evidence.trust_score = std::numeric_limits<double>::quiet_NaN();
    auto decision = gate.evaluate_telemetry(&evidence, 1);
    assert(decision.level == ailee::ev::GovernanceLevel::LEVEL_3_PROTECTIVE);
    assert(decision.overall_trust_score == 0.0);
    assert(!decision.learning_allowed);
    assert(!decision.evidence_valid);
    assert(decision.evidence_count == 1);

    ailee::ev::ParameterEnvelope envelope;
    assert(!gate.verify_parameter_update(0.95, envelope,
        std::numeric_limits<double>::quiet_NaN()));
    assert(!gate.verify_protective_mode_exit(
        std::numeric_limits<double>::quiet_NaN(), false, 100));

    ailee::ev::BMSCompartment bms;
    ailee::ev::BMSInputData bms_input;
    bms_input.temperature_c = std::numeric_limits<double>::infinity();
    bms.update_inputs(bms_input);
    assert(bms.evaluate(0.01).recommended_level ==
        ailee::ev::GovernanceLevel::LEVEL_3_PROTECTIVE);

    ds::drive::DSAileeTorqueManager torque_manager;
    ds::drive::TorqueCommand command;
    command.requested_torque_nm = std::numeric_limits<double>::quiet_NaN();
    auto torque = torque_manager.processTorqueCommand(command);
    assert(torque.applied_torque_nm == 0.0);
    assert(torque.governance_level == 3);
    assert(torque.trust_score == 0.0);
    std::cout << "  -> test_malformed_evidence_fails_closed PASSED.\n";
}

int main() {
    std::cout << "=== Running AILEE Trust Layer Integration Test Suite ===\n";
    test_trust_gated_learning();
    test_protective_mode_fallback();
    test_protective_mode_exit_recovery();
    test_rollback_mechanism();
    test_adapter_shims();
    test_malformed_evidence_fails_closed();
    std::cout << "=== ALL AILEE TRUST LAYER TESTS PASSED SUCCESSFULLY ===\n";
    return 0;
}
