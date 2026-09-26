/*
 * ============================================================================
 * AILEE TRUST LAYER — EV COMPARTMENT INTERFACE
 * ============================================================================
 *
 * Defines the abstract interface and core data types for modular EV subsystems
 * operating under the AILEE Trust Layer.
 *
 * All hot-path methods are designated noexcept and avoid dynamic heap allocation
 * to guarantee real-time execution in automotive embedded environments.
 *
 * Namespace: ailee::ev
 * License: MIT License. Copyright (c) Don Michael Feeney Jr.
 * ============================================================================
 */

#ifndef AILEE_TRUST_LAYER_COMPARTMENT_HPP
#define AILEE_TRUST_LAYER_COMPARTMENT_HPP

#include <cstdint>
#include <cstddef>

namespace ailee {
namespace ev {

/**
 * Discrete 4-tier AILEE Governance Levels for EV control.
 */
enum class GovernanceLevel : int32_t {
    LEVEL_0_NORMAL = 0,       // Full performance envelope (100% cap)
    LEVEL_1_SOFT_CEILING = 1, // Soft ceiling applied (90% cap)
    LEVEL_2_HARD_CEILING = 2, // Hard ceiling applied (65% cap)
    LEVEL_3_PROTECTIVE = 3    // Protective mode / conservative fallback (25% cap)
};

/**
 * Fixed-size status message string buffer (zero dynamic allocation).
 */
struct FixedMessage {
    char data[128] = {0};
};

/**
 * Standard telemetry snapshot returned by compartment evaluation.
 */
struct CompartmentTelemetry {
    double timestamp = 0.0;
    double health_score = 1.0;     // 0.0 .. 1.0
    double trust_score = 1.0;      // 0.0 .. 1.0
    double stress_metric = 0.0;    // Accumulated stress indicator
    bool anomaly_detected = false;
    GovernanceLevel recommended_level = GovernanceLevel::LEVEL_0_NORMAL;
    FixedMessage status_message;
};

/**
 * Parameter Envelope definition for bounding continuous learning updates.
 */
struct ParameterEnvelope {
    const char* param_name = "";
    double min_value = 0.0;
    double max_value = 1.0;
    double max_step_delta = 0.05; // Maximum allowed change per update cycle
    double current_value = 0.5;
    double default_value = 0.5;
};

/**
 * Abstract interface for all AILEE EV compartments.
 * Subsystems (BMS, Thermal, Charging, Anomaly Detection) implement this interface.
 */
class ICompartment {
public:
    virtual ~ICompartment() = default;

    /**
     * Unique human-readable compartment identifier.
     */
    virtual const char* get_name() const noexcept = 0;

    /**
     * Initialize compartment resources.
     */
    virtual bool initialize() noexcept = 0;

    /**
     * Evaluates compartment state for time step dt (seconds).
     * Must be real-time safe (zero heap allocation).
     */
    virtual CompartmentTelemetry evaluate(double dt) noexcept = 0;

    /**
     * Get the current recommended governance level based on internal safety checks.
     */
    virtual GovernanceLevel get_recommended_governance_level() const noexcept = 0;

    /**
     * Reset compartment to baseline safe state.
     */
    virtual void reset_to_baseline() noexcept = 0;
};

} // namespace ev
} // namespace ailee

#endif // AILEE_TRUST_LAYER_COMPARTMENT_HPP
