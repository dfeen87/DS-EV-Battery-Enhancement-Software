/*
 * ============================================================================
 * DS TORQUE MANAGER WITH AILEE GOVERNANCE INTEGRATION
 * ============================================================================
 *
 * Manages dynamic EV torque, horsepower ceilings, and battery discharge current
 * subject to AILEE Automotive Trust Layer governance evaluations.
 *
 * LICENSE: Copyright (c) Don Michael Feeney Jr. Licensed under the MIT License.
 * ============================================================================
 */

#ifndef DS_TORQUE_MANAGER_HPP
#define DS_TORQUE_MANAGER_HPP

#include "ailee_horsepower_governor.hpp"
#include "raps_ev_stability_membrane.hpp"
#include <memory>
#include <string>

namespace ds {
namespace drive {

struct alignas(64) TorqueCommand {
    double requested_torque_nm = 0.0;
    double motor_rpm = 0.0;
    double v_batt = 400.0;
    double i_batt = 0.0;
    GovernanceContext ctx;
};

struct alignas(64) GovernedTorqueOutput {
    double applied_torque_nm = 0.0;
    double applied_hp = 0.0;
    double max_allowed_torque_nm = 0.0;
    double max_allowed_current_a = 0.0;
    int governance_level = 0;
    double trust_score = 1.0;
    double hp_consistency_score = 1.0;
    double raps_membrane_stability = 1.0;
    double raps_boost_multiplier = 1.0;
    bool raps_dsm_tripped = false;
    std::string raps_dsm_trip_reason = "NONE";
    std::string reason;
    bool derating_active = false;
};

class DSAileeTorqueManager {
public:
    DSAileeTorqueManager();
    explicit DSAileeTorqueManager(std::shared_ptr<AileeHorsepowerGovernor> governor);
    ~DSAileeTorqueManager();

    GovernedTorqueOutput processTorqueCommand(const TorqueCommand& cmd);
    GovernanceDecisionCpp getLastGovernanceDecision() const;

private:
    std::shared_ptr<AileeHorsepowerGovernor> governor_;
    GovernanceDecisionCpp last_decision_;
    raps::ev::RapsEVStabilityMembrane raps_membrane_;
};

} // namespace drive
} // namespace ds

#endif // DS_TORQUE_MANAGER_HPP
