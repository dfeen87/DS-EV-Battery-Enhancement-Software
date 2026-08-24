/*
 * ============================================================================
 * DS TORQUE MANAGER IMPLEMENTATION
 * ============================================================================
 *
 * LICENSE: Copyright (c) Don Michael Feeney Jr. Licensed under the MIT License.
 * ============================================================================
 */

#include "ds_torque_manager.hpp"
#include <algorithm>
#include <cmath>

namespace ds {
namespace drive {

DSAileeTorqueManager::DSAileeTorqueManager()
    : governor_(std::make_shared<AileeHorsepowerGovernor>()) {}

DSAileeTorqueManager::DSAileeTorqueManager(std::shared_ptr<AileeHorsepowerGovernor> governor)
    : governor_(governor ? governor : std::make_shared<AileeHorsepowerGovernor>()) {}

DSAileeTorqueManager::~DSAileeTorqueManager() = default;

GovernedTorqueOutput DSAileeTorqueManager::processTorqueCommand(const TorqueCommand& cmd) {
    RawSignals signals;
    signals.torque_nm = std::max(0.0, cmd.requested_torque_nm);
    signals.rpm = std::max(1.0, cmd.motor_rpm);
    signals.v_batt = cmd.v_batt > 0.0 ? cmd.v_batt : 400.0;
    signals.i_batt = std::max(0.0, cmd.i_batt);
    signals.ctx = cmd.ctx;

    // Evaluate RAPS Stability Membrane
    auto raps_state = raps_membrane_.evaluate(signals.v_batt, signals.i_batt, signals.ctx.temp_c);

    // Evaluate governance decision from AILEE Trust Layer
    last_decision_ = governor_->evaluate(signals);

    GovernedTorqueOutput output;
    output.governance_level = last_decision_.level;
    output.trust_score = last_decision_.trust_score;
    output.hp_consistency_score = last_decision_.hp_consistency_score;
    output.raps_membrane_stability = raps_state.overall_membrane_stability;
    output.raps_boost_multiplier = raps_state.stability_boost_allowance;
    output.raps_dsm_tripped = raps_state.dsm_tripped;
    output.raps_dsm_trip_reason = raps_state.dsm_trip_reason;
    output.reason = last_decision_.reason;
    output.max_allowed_current_a = last_decision_.governed_discharge_current * raps_state.overall_membrane_stability;

    if (raps_state.dsm_tripped) {
        output.applied_torque_nm = 0.0;
        output.applied_hp = 0.0;
        output.max_allowed_torque_nm = 0.0;
        output.governance_level = 3;
        output.derating_active = true;
        output.reason = "RAPS_DSM_TRIP: " + raps_state.dsm_trip_reason;
        return output;
    }

    // Calculate governed HP base
    double governed_hp_base = last_decision_.governed_hp;
    // Apply RAPS extra boost allowance when stability and trust are high
    if (last_decision_.level == 0 && output.trust_score > 0.90 && raps_state.stability_boost_allowance > 1.0) {
        governed_hp_base *= raps_state.stability_boost_allowance;
    } else {
        governed_hp_base *= raps_state.overall_membrane_stability;
    }

    // Calculate maximum allowed torque from governed HP ceiling at current RPM
    double torque_ceiling_from_hp = (governed_hp_base * 7121.23) / signals.rpm;
    output.max_allowed_torque_nm = std::min(last_decision_.governed_torque * raps_state.overall_membrane_stability, torque_ceiling_from_hp);

    // Apply level-based derating and clamp requested torque to governed limit
    output.applied_torque_nm = std::min(signals.torque_nm, output.max_allowed_torque_nm);

    // Derating flag indicates requested torque or HP exceeds governed envelope
    output.derating_active = (signals.torque_nm > output.applied_torque_nm) || (last_decision_.level > 0) || (raps_state.overall_membrane_stability < 0.95);

    // Calculate applied mechanical horsepower
    output.applied_hp = AileeHorsepowerGovernor::computeMechanicalHp(output.applied_torque_nm, signals.rpm);

    return output;
}

GovernanceDecisionCpp DSAileeTorqueManager::getLastGovernanceDecision() const {
    return last_decision_;
}

} // namespace drive
} // namespace ds
