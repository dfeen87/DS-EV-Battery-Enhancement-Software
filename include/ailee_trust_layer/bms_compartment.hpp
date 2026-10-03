/*
 * ============================================================================
 * AILEE TRUST LAYER — BMS SUBSYSTEM COMPARTMENT
 * ============================================================================
 *
 * Implements the Battery Management System (BMS) compartment under the AILEE Trust Layer.
 * Integrates dual-state physical metrics, cell imbalance tracking, and thermal safety checks.
 *
 * Namespace: ailee::ev
 * License: MIT License. Copyright (c) Don Michael Feeney Jr.
 * ============================================================================
 */

#ifndef AILEE_TRUST_LAYER_BMS_COMPARTMENT_HPP
#define AILEE_TRUST_LAYER_BMS_COMPARTMENT_HPP

#include "ailee_compartment.hpp"
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace ailee {
namespace ev {

struct BMSInputData {
    double pack_voltage = 400.0;
    double pack_current = 0.0;
    double temperature_c = 25.0;
    double soc = 100.0;
    double soh = 100.0;
    double cell_min_v = 4.15;
    double cell_max_v = 4.17;
    bool sensor_valid = true;
};

class BMSCompartment : public ICompartment {
public:
    BMSCompartment() noexcept {
        reset_to_baseline();
    }

    const char* get_name() const noexcept override {
        return "BMSCompartment";
    }

    bool initialize() noexcept override {
        reset_to_baseline();
        return true;
    }

    void update_inputs(const BMSInputData& data) noexcept {
        inputs_ = data;
    }

    CompartmentTelemetry evaluate(double dt) noexcept override {
        (void)dt;
        CompartmentTelemetry t;
        t.timestamp += dt;

        if (!inputs_.sensor_valid || !std::isfinite(inputs_.pack_voltage) ||
            !std::isfinite(inputs_.pack_current) || !std::isfinite(inputs_.temperature_c) ||
            !std::isfinite(inputs_.soc) || !std::isfinite(inputs_.soh) ||
            !std::isfinite(inputs_.cell_min_v) || !std::isfinite(inputs_.cell_max_v) ||
            inputs_.pack_voltage <= 0.0 || inputs_.soc < 0.0 || inputs_.soc > 100.0 ||
            inputs_.soh < 0.0 || inputs_.soh > 100.0 || inputs_.cell_min_v < 0.0 ||
            inputs_.cell_max_v < inputs_.cell_min_v) {
            t.health_score = 0.0;
            t.trust_score = 0.0;
            t.anomaly_detected = true;
            t.recommended_level = GovernanceLevel::LEVEL_3_PROTECTIVE;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "BMS sensor validity check failed.");
            return t;
        }

        // Calculate cell imbalance
        double v_imbalance = inputs_.cell_max_v - inputs_.cell_min_v;
        double trust = 1.0;

        if (inputs_.temperature_c > 45.0) {
            trust -= (inputs_.temperature_c - 45.0) * 0.025;
        }
        if (inputs_.soc < 20.0) {
            trust -= (20.0 - inputs_.soc) * 0.02;
        }
        if (inputs_.soh < 85.0) {
            trust -= (85.0 - inputs_.soh) * 0.015;
        }
        if (v_imbalance > 0.05) { // 50mV imbalance warning
            trust -= (v_imbalance - 0.05) * 5.0;
        }

        trust = std::max(0.0, std::min(1.0, trust));
        t.trust_score = trust;
        t.health_score = std::max(0.0, std::min(1.0, inputs_.soh / 100.0));
        t.stress_metric = (inputs_.temperature_c > 35.0 ? (inputs_.temperature_c - 35.0) * 0.1 : 0.0) + (v_imbalance * 10.0);

        if (inputs_.temperature_c >= 60.0 || inputs_.soc <= 5.0 || trust < 0.50) {
            t.recommended_level = GovernanceLevel::LEVEL_3_PROTECTIVE;
            t.anomaly_detected = (inputs_.temperature_c >= 65.0 || v_imbalance > 0.15);
            snprintf(t.status_message.data, sizeof(t.status_message.data), "BMS Critical: High temp or severe discharge.");
        } else if (inputs_.temperature_c >= 50.0 || inputs_.soc <= 15.0 || trust < 0.70) {
            t.recommended_level = GovernanceLevel::LEVEL_2_HARD_CEILING;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "BMS Warning: Hard limit applied.");
        } else if (inputs_.temperature_c >= 42.0 || inputs_.soc <= 25.0 || trust < 0.85) {
            t.recommended_level = GovernanceLevel::LEVEL_1_SOFT_CEILING;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "BMS Advisory: Soft limit applied.");
        } else {
            t.recommended_level = GovernanceLevel::LEVEL_0_NORMAL;
            snprintf(t.status_message.data, sizeof(t.status_message.data), "BMS Normal operation.");
        }

        return t;
    }

    GovernanceLevel get_recommended_governance_level() const noexcept override {
        BMSCompartment mutable_this = *this;
        auto t = mutable_this.evaluate(0.0);
        return t.recommended_level;
    }

    void reset_to_baseline() noexcept override {
        inputs_ = BMSInputData();
    }

private:
    BMSInputData inputs_;
};

} // namespace ev
} // namespace ailee

#endif // AILEE_TRUST_LAYER_BMS_COMPARTMENT_HPP
