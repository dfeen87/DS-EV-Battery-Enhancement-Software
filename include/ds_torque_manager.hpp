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
#include <memory>
#include <string>

namespace ds {
namespace drive {

struct TorqueCommand {
    double requested_torque_nm = 0.0;
    double motor_rpm = 0.0;
    double v_batt = 400.0;
    double i_batt = 0.0;
    GovernanceContext ctx;
};

struct GovernedTorqueOutput {
    double applied_torque_nm = 0.0;
    double applied_hp = 0.0;
    double max_allowed_torque_nm = 0.0;
    double max_allowed_current_a = 0.0;
    int governance_level = 0;
    double trust_score = 1.0;
    double hp_consistency_score = 1.0;
    std::string reason;
    bool derating_active = false;
};

class DSTorqueManager {
public:
    DSTorqueManager();
    explicit DSTorqueManager(std::shared_ptr<AileeHorsepowerGovernor> governor);
    ~DSTorqueManager();

    GovernedTorqueOutput processTorqueCommand(const TorqueCommand& cmd);
    GovernanceDecisionCpp getLastGovernanceDecision() const;

private:
    std::shared_ptr<AileeHorsepowerGovernor> governor_;
    GovernanceDecisionCpp last_decision_;
};

} // namespace drive
} // namespace ds

#endif // DS_TORQUE_MANAGER_HPP
