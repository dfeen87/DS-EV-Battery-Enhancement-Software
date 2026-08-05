/*
 * ============================================================================
 * DS Energy Recovery Coordinator v1.0
 * ============================================================================
 *
 * PURPOSE:
 *   Close the EV energy loop by coordinating regen braking ↔ battery charging.
 *
 *   This module:
 *     1) Takes regen torque limits from the Regen Braking Manager
 *     2) Converts allowed regen torque into an estimated electrical regen power/current
 *     3) Feeds the result into DSBMSMiddleware as a charging event (negative current)
 *     4) Tracks recovered energy (kWh) and exposes clean diagnostics
 *
 * WHAT THIS MODULE IS:
 *   - A deterministic coordinator between braking control and battery state estimation.
 *   - A safe place to enforce sign conventions and energy accounting.
 *
 * WHAT THIS MODULE IS NOT:
 *   - It is not a motor/inverter model.
 *   - It does not override OEM safety controllers.
 *   - It does not replace current sensors (it can use estimated current only if needed).
 *
 * OEM NOTE:
 *   Best practice is to use actual measured pack current (from sensors) as truth.
 *   This coordinator supports both:
 *     (A) "Measured-current mode" (preferred)
 *     (B) "Estimated-current mode" (simulation / early integration / missing sensors)
 *
 * AUTHORS: Don Michael Feeney Jr. + Lex
 * DATE: December 2025
 * LICENSE: Licensed under the PolyForm Noncommercial License 1.0.0
 * VERSION: 1.0.0
 * ============================================================================
 */

#ifndef DS_ENERGY_RECOVERY_COORDINATOR_V1_HPP
#define DS_ENERGY_RECOVERY_COORDINATOR_V1_HPP

#include "ds_bms_middleware_v2.hpp"
#include "ds_regen_braking_manager_v1.hpp"
#include <string>
#include <algorithm>
#include <cmath>

namespace ds {
namespace drive {

constexpr int RECOVERY_VERSION_MAJOR = 1;
constexpr int RECOVERY_VERSION_MINOR = 0;
constexpr int RECOVERY_VERSION_PATCH = 0;

inline std::string energy_recovery_version() {
    return std::to_string(RECOVERY_VERSION_MAJOR) + "." +
           std::to_string(RECOVERY_VERSION_MINOR) + "." +
           std::to_string(RECOVERY_VERSION_PATCH);
}

struct EnergyRecoveryConfig {
    // If true, you provide measured pack_current to BMS; coordinator only logs recovery.
    // If false, coordinator estimates pack_current from regen torque (useful for simulation).
    bool prefer_measured_pack_current = true;

    // Motor/inverter estimation parameters (only used if prefer_measured_pack_current == false)
    // These are intentionally conservative approximations.
    double drivetrain_efficiency_regen = 0.85;     // electrical recovery efficiency
    double max_estimated_regen_current_a = 250.0;  // sanity clamp for estimated current
    double min_pack_voltage_v = 50.0;              // avoid divide-by-zero

    // Optional smoothing for estimated current
    double current_estimate_tau_s = 0.10;

    // Regen torque to mechanical power conversion:
    // P_mech = T * ω
    // ω must be rad/s. If you only have vehicle speed, OEM should translate speed to motor omega upstream.
};

struct EnergyRecoveryDiagnostics {
    double last_regen_torque_nm = 0.0;
    double last_regen_power_kw_est = 0.0;
    double last_regen_current_a_est = 0.0;

    double recovered_energy_kwh_total = 0.0;
    double recovered_energy_kwh_session = 0.0;

    std::string mode = "MEASURED_CURRENT";
    std::string limiting_factor = "NONE";
};

struct EnergyRecoveryResult {
    // What we used to update the BMS (either measured or estimated)
    double pack_voltage_v = 0.0;
    double pack_current_a_used = 0.0;     // signed: negative = charging (regen)
    double pack_temperature_c = 0.0;
    double soc_input = 0.0;
    double dt_s = 0.0;

    // Regen metrics
    double regen_torque_nm = 0.0;
    double regen_power_kw_est = 0.0;

    // Updated enhanced state + diagnostics
    ds::EnhancedState enhanced;
    ds_plugin::DiagnosticReport bms_diag;

    EnergyRecoveryDiagnostics diag;
};

class DSEnergyRecoveryCoordinator {
public:
    explicit DSEnergyRecoveryCoordinator(const EnergyRecoveryConfig& cfg = EnergyRecoveryConfig())
        : cfg_(cfg), est_current_a_(0.0) {}

    static std::string get_version() { return energy_recovery_version(); }

    // Core step:
    //   - Accept regen decision (torque limit + regen fraction)
    //   - Update BMS using measured or estimated pack current
    //
    // Inputs:
    //   bms: DSBMSMiddleware instance
    //   enhanced_prev: last enhanced state (optional; not required)
    //   regen: regen result from DSRegenBrakingManager
    //   pack_voltage_v, pack_temperature_c: measured pack values
    //   pack_current_a_measured: measured pack current (signed) if available
    //   soc_input: OEM SOC estimate or last SOC (0..1)
    //   motor_omega_rad_s: motor electrical/mechanical omega in rad/s (if estimating current)
    //
    EnergyRecoveryResult step(
        ds_plugin::DSBMSMiddleware& bms,
        const ds::drive::RegenResult& regen,
        double pack_voltage_v,
        double pack_temperature_c,
        double pack_current_a_measured,
        double soc_input,
        double motor_omega_rad_s,
        double dt_s
    ) {
        EnergyRecoveryResult out;
        out.pack_voltage_v = pack_voltage_v;
        out.pack_temperature_c = pack_temperature_c;
        out.soc_input = clamp01(soc_input);
        out.dt_s = std::max(1e-6, dt_s);

        // Estimate regen mechanical power (only meaningful if omega supplied)
        const double regen_torque_nm = std::max(0.0, regen.max_regen_torque_nm);
        out.regen_torque_nm = regen_torque_nm;

        double p_mech_w = regen_torque_nm * std::max(0.0, motor_omega_rad_s);
        double p_elec_w = p_mech_w * cfg_.drivetrain_efficiency_regen;
        out.regen_power_kw_est = p_elec_w / 1000.0;

        // Determine pack current to use for BMS update
        double current_used = pack_current_a_measured; // default to measured

        EnergyRecoveryDiagnostics d;
        d.last_regen_torque_nm = regen_torque_nm;
        d.last_regen_power_kw_est = out.regen_power_kw_est;
        d.limiting_factor = regen.limiting_factor;

        if (!cfg_.prefer_measured_pack_current) {
            d.mode = "ESTIMATED_CURRENT";

            // Convert electrical power to current: I = P / V  (charging current is negative by convention)
            const double v = std::max(cfg_.min_pack_voltage_v, std::abs(pack_voltage_v));
            double i_est = (p_elec_w / v);
            i_est = std::clamp(i_est, 0.0, cfg_.max_estimated_regen_current_a);

            // Sign convention: regen charges the pack => negative current
            i_est = -i_est;

            // Smooth estimate to avoid jitter
            current_used = smooth_current(i_est, out.dt_s);
            d.last_regen_current_a_est = current_used;
        } else {
            d.mode = "MEASURED_CURRENT";
            // OEM NOTE:
            //   In a real EV, pack_current_a_measured already includes regen charging.
            //   The regen torque limit is still valuable for controlling what current will be.
            d.last_regen_current_a_est = 0.0;
        }

        out.pack_current_a_used = current_used;
        out.diag = d;

        // Update BMS with the chosen current
        out.enhanced = bms.enhance_cycle(
            pack_voltage_v,
            current_used,
            pack_temperature_c,
            out.soc_input,
            out.dt_s
        );
        out.bms_diag = bms.get_diagnostics();

        // Energy accounting: only count recovery if current indicates charging or estimated regen > 0
        // In measured-current mode, count based on measured sign to avoid double counting.
        double p_kw_for_accounting = (pack_voltage_v * current_used) / 1000.0; // charging negative
        double recovered_kwh = 0.0;

        if (cfg_.prefer_measured_pack_current) {
            if (current_used < 0.0) {
                recovered_kwh = std::abs(p_kw_for_accounting) * out.dt_s / 3600.0;
            }
        } else {
            recovered_kwh = std::max(0.0, out.regen_power_kw_est) * out.dt_s / 3600.0;
        }

        diag_.recovered_energy_kwh_total += recovered_kwh;
        diag_.recovered_energy_kwh_session += recovered_kwh;

        // Update persistent diag snapshot
        diag_.last_regen_torque_nm = d.last_regen_torque_nm;
        diag_.last_regen_power_kw_est = d.last_regen_power_kw_est;
        diag_.last_regen_current_a_est = d.last_regen_current_a_est;
        diag_.mode = d.mode;
        diag_.limiting_factor = d.limiting_factor;

        out.diag = diag_;
        return out;
    }

    const EnergyRecoveryDiagnostics& diagnostics() const { return diag_; }

    void reset_session_energy() { diag_.recovered_energy_kwh_session = 0.0; }

private:
    EnergyRecoveryConfig cfg_;
    double est_current_a_;
    EnergyRecoveryDiagnostics diag_;

    static double clamp01(double x) { return std::clamp(x, 0.0, 1.0); }

    double smooth_current(double target, double dt) {
        const double tau = std::max(1e-3, cfg_.current_estimate_tau_s);
        const double alpha = 1.0 - std::exp(-dt / tau);
        est_current_a_ = est_current_a_ + alpha * (target - est_current_a_);
        return est_current_a_;
    }
};

} // namespace drive
} // namespace ds

#endif // DS_ENERGY_RECOVERY_COORDINATOR_V1_HPP
