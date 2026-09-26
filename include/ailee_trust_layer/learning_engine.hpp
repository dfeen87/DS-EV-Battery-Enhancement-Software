/*
 * ============================================================================
 * AILEE TRUST LAYER — BOUNDED CONTINUOUS LEARNING ENGINE
 * ============================================================================
 *
 * Implements continuous learning mechanisms, envelope verification, parameter
 * snapshot creation, audit logging, and snapshot rollback handles.
 *
 * All parameter adaptations are gated by the TrustGate (trust_score >= 0.85).
 * Updates are snapshotted to allow complete auditability and instant rollback.
 *
 * Namespace: ailee::ev
 * License: MIT License. Copyright (c) Don Michael Feeney Jr.
 * ============================================================================
 */

#ifndef AILEE_TRUST_LAYER_LEARNING_ENGINE_HPP
#define AILEE_TRUST_LAYER_LEARNING_ENGINE_HPP

#include "ailee_compartment.hpp"
#include "ailee_trust_gate.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <ctime>
#include <cstdio>

namespace ailee {
namespace ev {

struct ParameterSnapshot {
    uint64_t snapshot_id = 0;
    std::string timestamp;
    std::string compartment_name;
    std::string param_name;
    double previous_value = 0.0;
    double proposed_value = 0.0;
    double applied_value = 0.0;
    double trust_score = 0.0;
    bool approved = false;
    std::string reason;
};

class LearningEngine {
public:
    explicit LearningEngine(const std::string& audit_log_path = "logs/ailee_learning_audit.json",
                           const std::string& automotive_log_path = "logs/ailee_automotive_audit.log")
        : audit_log_path_(audit_log_path), automotive_log_path_(automotive_log_path), next_snapshot_id_(1) {}

    /**
     * Submit a proposed parameter update for a compartment.
     * Evaluates trust gate and parameter envelope.
     */
    bool propose_parameter_update(const std::string& compartment_name,
                                 ParameterEnvelope& envelope,
                                 double proposed_value,
                                 double current_trust_score,
                                 const TrustGate& trust_gate,
                                 ParameterSnapshot* out_snapshot = nullptr) {
        bool approved = trust_gate.verify_parameter_update(current_trust_score, envelope, proposed_value);

        ParameterSnapshot snap;
        snap.snapshot_id = next_snapshot_id_++;
        snap.timestamp = get_current_iso_timestamp();
        snap.compartment_name = compartment_name;
        snap.param_name = envelope.param_name;
        snap.previous_value = envelope.current_value;
        snap.proposed_value = proposed_value;
        snap.trust_score = current_trust_score;
        snap.approved = approved;

        if (approved) {
            envelope.current_value = proposed_value;
            snap.applied_value = proposed_value;
            snap.reason = "Approved: Trust score >= 0.85 and envelope delta check passed.";
        } else {
            snap.applied_value = envelope.current_value;
            if (current_trust_score < 0.85) {
                snap.reason = "Rejected: Trust score (" + std::to_string(current_trust_score) + ") below threshold 0.85.";
            } else {
                snap.reason = "Rejected: Proposed value (" + std::to_string(proposed_value) + ") violates envelope limits or max step delta.";
            }
        }

        snapshots_.push_back(snap);
        write_audit_log(snap);

        if (out_snapshot != nullptr) {
            *out_snapshot = snap;
        }

        return approved;
    }

    /**
     * Rollback a specific snapshot ID, restoring previous_value into the envelope.
     */
    bool rollback(uint64_t snapshot_id, ParameterEnvelope& envelope) {
        for (auto it = snapshots_.rbegin(); it != snapshots_.rend(); ++it) {
            if (it->snapshot_id == snapshot_id && it->approved) {
                envelope.current_value = it->previous_value;

                ParameterSnapshot snap;
                snap.snapshot_id = next_snapshot_id_++;
                snap.timestamp = get_current_iso_timestamp();
                snap.compartment_name = it->compartment_name;
                snap.param_name = it->param_name;
                snap.previous_value = it->applied_value;
                snap.proposed_value = it->previous_value;
                snap.applied_value = it->previous_value;
                snap.trust_score = 1.0;
                snap.approved = true;
                snap.reason = "ROLLBACK EXECUTION for snapshot ID " + std::to_string(snapshot_id);

                snapshots_.push_back(snap);
                write_audit_log(snap);
                return true;
            }
        }
        return false;
    }

    /**
     * Get snapshot history.
     */
    const std::vector<ParameterSnapshot>& get_snapshots() const noexcept {
        return snapshots_;
    }

private:
    std::string get_current_iso_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_now));
        return std::string(buf);
    }

    void write_audit_log(const ParameterSnapshot& snap) {
        // Write to JSON audit log
        FILE* f_json = std::fopen(audit_log_path_.c_str(), "a");
        if (f_json != nullptr) {
            std::fprintf(f_json,
                "{\"snapshot_id\":%llu,\"timestamp\":\"%s\",\"compartment\":\"%s\",\"param\":\"%s\",\"previous\":%.6f,\"proposed\":%.6f,\"applied\":%.6f,\"trust_score\":%.4f,\"approved\":%s,\"reason\":\"%s\"}\n",
                (unsigned long long)snap.snapshot_id,
                snap.timestamp.c_str(),
                snap.compartment_name.c_str(),
                snap.param_name.c_str(),
                snap.previous_value,
                snap.proposed_value,
                snap.applied_value,
                snap.trust_score,
                snap.approved ? "true" : "false",
                snap.reason.c_str());
            std::fclose(f_json);
        }

        // Write to automotive audit log
        FILE* f_auto = std::fopen(automotive_log_path_.c_str(), "a");
        if (f_auto != nullptr) {
            std::fprintf(f_auto,
                "[%s] [AILEE_LEARNING] Snapshot #%llu | Compartment: %s | Param: %s | Val: %.4f -> %.4f | Trust: %.2f | Approved: %s | %s\n",
                snap.timestamp.c_str(),
                (unsigned long long)snap.snapshot_id,
                snap.compartment_name.c_str(),
                snap.param_name.c_str(),
                snap.previous_value,
                snap.applied_value,
                snap.trust_score,
                snap.approved ? "YES" : "NO",
                snap.reason.c_str());
            std::fclose(f_auto);
        }
    }

    std::string audit_log_path_;
    std::string automotive_log_path_;
    uint64_t next_snapshot_id_;
    std::vector<ParameterSnapshot> snapshots_;
};

} // namespace ev
} // namespace ailee

#endif // AILEE_TRUST_LAYER_LEARNING_ENGINE_HPP
