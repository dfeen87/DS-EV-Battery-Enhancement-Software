/*
 * ============================================================================
 * AILEE TRUST LAYER — ANOMALY DETECTION COMPARTMENT
 * ============================================================================
 *
 * Implements cross-signal anomaly verification (e.g. Mechanical vs. Electrical HP),
 * sensor signal plausibility checks, and failure mode detection.
 *
 * Namespace: ailee::ev
 * License: MIT License. Copyright (c) Don Michael Feeney Jr.
 * ============================================================================
 */

#ifndef AILEE_TRUST_LAYER_ANOMALY_COMPARTMENT_HPP
#define AILEE_TRUST_LAYER_ANOMALY_COMPARTMENT_HPP

#include "ailee_compartment.hpp"
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace ailee {
namespace ev {

struct AnomalyInputData {
    double torque_nm = 0.0;
    double rpm = 0.0;
    double v_batt = 400.0;
    double i_batt = 0.0;
    bool sensor_valid = true;
};

class AnomalyCompartment : public ICompartment {
public:
    AnomalyCompartment() noexcept {
        reset_to_baseline();
    }

    const char* get_name() const noexcept override {
        return "AnomalyCompartment";
    }

    bool initialize() noexcept override {
        reset_to_baseline();
        return true;
    }

    void update_inputs(const AnomalyInputData& data) noexcept {
        inputs_ = data;
    }

    CompartmentTelemetry evaluate(double dt) noexcept override {
        (void)dt;
        CompartmentTelemetry t;

        if (!inputs_.sensor_valid) {
            t.health_score = 0.0;
            t.trust_score = 0.0;
            t.anomaly_detected = true;
            t.recommended_level = GovernanceLevel::LEVEL_3_PROTECTIVE;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "Anomaly Compartment: Sensor invalid.");
            return t;
        }

        // Compute Mechanical and Electrical HP
        double hp_mech = (inputs_.torque_nm * inputs_.rpm) / 7121.23;
        double hp_elec = (inputs_.v_batt * inputs_.i_batt) / 745.7;

        double consistency = 1.0;
        double max_hp = std::max(std::abs(hp_mech), std::abs(hp_elec));
        if (max_hp > 10.0) { // Evaluate consistency above 10 HP
            double diff = std::abs(hp_mech - hp_elec);
            double ratio = diff / max_hp;
            consistency = std::max(0.0, 1.0 - ratio);
        }

        double trust = consistency;
        t.trust_score = trust;
        t.health_score = consistency;

        if (consistency < 0.50) {
            t.anomaly_detected = true;
            t.recommended_level = GovernanceLevel::LEVEL_3_PROTECTIVE;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "HP Divergence Anomaly (consistency=%.2f)", consistency);
        } else if (consistency < 0.70) {
            t.anomaly_detected = false;
            t.recommended_level = GovernanceLevel::LEVEL_2_HARD_CEILING;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "HP Mismatch Warning (consistency=%.2f)", consistency);
        } else if (consistency < 0.85) {
            t.anomaly_detected = false;
            t.recommended_level = GovernanceLevel::LEVEL_1_SOFT_CEILING;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "HP Slight Mismatch (consistency=%.2f)", consistency);
        } else {
            t.anomaly_detected = false;
            t.recommended_level = GovernanceLevel::LEVEL_0_NORMAL;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "Signals consistent.");
        }

        return t;
    }

    GovernanceLevel get_recommended_governance_level() const noexcept override {
        AnomalyCompartment mutable_this = *this;
        auto t = mutable_this.evaluate(0.0);
        return t.recommended_level;
    }

    void reset_to_baseline() noexcept override {
        inputs_ = AnomalyInputData();
    }

private:
    AnomalyInputData inputs_;
};

} // namespace ev
} // namespace ailee

#endif // AILEE_TRUST_LAYER_ANOMALY_COMPARTMENT_HPP
