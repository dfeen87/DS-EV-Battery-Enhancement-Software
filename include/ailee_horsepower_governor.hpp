/*
 * ============================================================================
 * AILEE TRUST LAYER AUTOMOTIVE DOMAIN HORSEPOWER GOVERNOR
 * ============================================================================
 *
 * Provides a clean C++ API for governing Mechanical and Electrical Horsepower,
 * calculating consistency scores, and executing AILEE automotive governance policies.
 *
 * LICENSE: Copyright (c) Don Michael Feeney Jr. Licensed under the MIT License.
 * ============================================================================
 */

#ifndef AILEE_HORSEPOWER_GOVERNOR_HPP
#define AILEE_HORSEPOWER_GOVERNOR_HPP

#include <string>
#include <memory>
#include <algorithm>
#include <cmath>

struct GovernanceContext {
    double soc = 100.0;
    double soh = 100.0;
    double temp_c = 25.0;
    bool sensor_valid = true;
};

struct RawSignals {
    double torque_nm = 0.0;
    double rpm = 0.0;
    double v_batt = 0.0;
    double i_batt = 0.0;
    GovernanceContext ctx;
};

struct GovernanceDecisionCpp {
    int level = 0; // 0: Normal, 1: Soft Ceiling, 2: Hard Ceiling, 3: Protective Mode
    double governed_hp = 0.0;
    double governed_torque = 0.0;
    double governed_discharge_current = 0.0;
    double trust_score = 1.0;
    double hp_consistency_score = 1.0;
    double hp_mech = 0.0;
    double hp_elec = 0.0;
    std::string reason;
    bool used_fallback = false;
};

class AileeHorsepowerGovernor {
public:
    AileeHorsepowerGovernor();
    ~AileeHorsepowerGovernor();

    double governHorsepower(double raw_hp_mech, double raw_hp_elec, const GovernanceContext& ctx);
    GovernanceDecisionCpp evaluate(const RawSignals& signals);

    static double computeMechanicalHp(double torque_nm, double rpm);
    static double computeElectricalHp(double v_batt, double i_batt);
    static double hpConsistencyScore(double hp_mech, double hp_elec);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // AILEE_HORSEPOWER_GOVERNOR_HPP
