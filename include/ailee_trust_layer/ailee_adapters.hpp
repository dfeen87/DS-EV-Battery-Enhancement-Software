/*
 * ============================================================================
 * AILEE TRUST LAYER — ADAPTER SHIMS FOR BACKWARD COMPATIBILITY
 * ============================================================================
 *
 * Provides adapter classes bridging legacy DS components (DSEnhancement,
 * DSTorqueManager, DSBMSMiddleware, AileeHorsepowerGovernor) to the unified
 * ailee::ev compartment architecture.
 *
 * Namespace: ailee::ev::adapters
 * License: MIT License. Copyright (c) Don Michael Feeney Jr.
 * ============================================================================
 */

#ifndef AILEE_TRUST_LAYER_ADAPTERS_HPP
#define AILEE_TRUST_LAYER_ADAPTERS_HPP

#include "ailee_compartment.hpp"
#include "ailee_trust_gate.hpp"
#include "bms_compartment.hpp"
#include "anomaly_compartment.hpp"
#include "../ailee_horsepower_governor.hpp"
#include "../ds_battery_enhancement.hpp"
#include "../ds_bms_middleware_v2.hpp"

#include <cstdio>
#include <memory>

namespace ailee {
namespace ev {
namespace adapters {

/**
 * Adapter wrapping AileeHorsepowerGovernor as an AILEE Trust Layer Anomaly Compartment.
 */
class HorsepowerGovernorAdapter : public ICompartment {
public:
    HorsepowerGovernorAdapter() = default;

    const char* get_name() const noexcept override {
        return "HorsepowerGovernorAdapter";
    }

    bool initialize() noexcept override {
        governor_ = std::make_unique<AileeHorsepowerGovernor>();
        return true;
    }

    void set_raw_signals(const RawSignals& signals) noexcept {
        signals_ = signals;
    }

    CompartmentTelemetry evaluate(double dt) noexcept override {
        (void)dt;
        CompartmentTelemetry t;
        if (!governor_) {
            initialize();
        }
        GovernanceDecisionCpp dec = governor_->evaluate(signals_);
        t.trust_score = dec.trust_score;
        t.health_score = dec.hp_consistency_score;
        t.anomaly_detected = (dec.level == 3);
        t.recommended_level = static_cast<GovernanceLevel>(dec.level);
        snprintf(t.status_message.data, sizeof(t.status_message.data), "%s", dec.reason.c_str());
        return t;
    }

    GovernanceLevel get_recommended_governance_level() const noexcept override {
        if (!governor_) return GovernanceLevel::LEVEL_3_PROTECTIVE;
        GovernanceDecisionCpp dec = governor_->evaluate(signals_);
        return static_cast<GovernanceLevel>(dec.level);
    }

    void reset_to_baseline() noexcept override {
        signals_ = RawSignals();
    }

private:
    std::unique_ptr<AileeHorsepowerGovernor> governor_;
    RawSignals signals_;
};

/**
 * Adapter wrapping DSBMSMiddleware as an AILEE Trust Layer BMS Compartment.
 */
class BMSMiddlewareAdapter : public ICompartment {
public:
    BMSMiddlewareAdapter() = default;

    const char* get_name() const noexcept override {
        return "BMSMiddlewareAdapter";
    }

    bool initialize() noexcept override {
        bms_middleware_.init(75.0, 400.0);
        return true;
    }

    void update_cycle(double voltage, double current, double temperature, double soc, double dt) {
        bms_middleware_.enhance_cycle(voltage, current, temperature, soc, dt);
    }

    CompartmentTelemetry evaluate(double dt) noexcept override {
        (void)dt;
        CompartmentTelemetry t;
        auto diag = bms_middleware_.get_diagnostics();
        t.health_score = diag.pack_health_percent / 100.0;
        t.trust_score = diag.safety_fault ? 0.0 : (diag.pack_health_percent / 100.0);
        t.anomaly_detected = diag.safety_fault || diag.thermal_warning;
        t.recommended_level = t.anomaly_detected ? GovernanceLevel::LEVEL_3_PROTECTIVE : GovernanceLevel::LEVEL_0_NORMAL;
        snprintf(t.status_message.data, sizeof(t.status_message.data), "BMS Middleware Health: %.2f%%", diag.pack_health_percent);
        return t;
    }

    GovernanceLevel get_recommended_governance_level() const noexcept override {
        auto diag = bms_middleware_.get_diagnostics();
        if (diag.safety_fault || diag.thermal_warning) {
            return GovernanceLevel::LEVEL_3_PROTECTIVE;
        }
        return GovernanceLevel::LEVEL_0_NORMAL;
    }

    void reset_to_baseline() noexcept override {
        initialize();
    }

private:
    ds_plugin::DSBMSMiddleware bms_middleware_;
};

} // namespace adapters
} // namespace ev
} // namespace ailee

#endif // AILEE_TRUST_LAYER_ADAPTERS_HPP
