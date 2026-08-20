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

DSTorqueManager::DSTorqueManager()
    : governor_(std::make_shared<AileeHorsepowerGovernor>()) {}

DSTorqueManager::DSTorqueManager(std::shared_ptr<AileeHorsepowerGovernor> governor)
    : governor_(governor ? governor : std::make_shared<AileeHorsepowerGovernor>()) {}

DSTorqueManager::~DSTorqueManager() = default;

GovernedTorqueOutput DSTorqueManager::processTorqueCommand(const TorqueCommand& cmd) {
    RawSignals signals;
    signals.torque_nm = std::max(0.0, cmd.requested_torque_nm);
    signals.rpm = std::max(1.0, cmd.motor_rpm);
    signals.v_batt = cmd.v_batt > 0.0 ? cmd.v_batt : 400.0;
    signals.i_batt = std::max(0.0, cmd.i_batt);
    signals.ctx = cmd.ctx;

    // Evaluate governance decision from AILEE Trust Layer
    last_decision_ = governor_->evaluate(signals);

    GovernedTorqueOutput output;
    output.governance_level = last_decision_.level;
    output.trust_score = last_decision_.trust_score;
    output.hp_consistency_score = last_decision_.hp_consistency_score;
    output.reason = last_decision_.reason;
    output.max_allowed_current_a = last_decision_.governed_discharge_current;

    // Calculate maximum allowed torque from governed HP ceiling at current RPM
    // Formula: Torque_Nm = (HP * 7121.23) / RPM
    double torque_ceiling_from_hp = (last_decision_.governed_hp * 7121.23) / signals.rpm;
    output.max_allowed_torque_nm = std::min(last_decision_.governed_torque, torque_ceiling_from_hp);

    // Apply level-based derating and clamp requested torque to governed limit
    output.applied_torque_nm = std::min(signals.torque_nm, output.max_allowed_torque_nm);

    // Derating flag indicates requested torque or HP exceeds governed envelope
    output.derating_active = (signals.torque_nm > output.applied_torque_nm) || (last_decision_.level > 0);

    // Calculate applied mechanical horsepower
    output.applied_hp = AileeHorsepowerGovernor::computeMechanicalHp(output.applied_torque_nm, signals.rpm);

    return output;
}

GovernanceDecisionCpp DSTorqueManager::getLastGovernanceDecision() const {
    return last_decision_;
}

} // namespace drive
} // namespace ds
