#ifndef BATTERY_FEEN_ADAPTER_HPP
#define BATTERY_FEEN_ADAPTER_HPP

#ifndef DS_ENABLE_FEEN
#define DS_ENABLE_FEEN 0
#endif

#if DS_ENABLE_FEEN
#include <feen/ailee/confidence.h>
#endif

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace ds {

class BatteryFeenAdapter {
private:
#if DS_ENABLE_FEEN
    feen::ailee::PhononicConfidenceScorer scorer_;
#endif
    std::vector<double> voltage_history_;
    size_t max_history_size_;

public:
#if DS_ENABLE_FEEN
    BatteryFeenAdapter(size_t max_history_size = 10)
        : scorer_(feen::ailee::ConfidenceConfig{}),
          max_history_size_(max_history_size) {}
#else
    BatteryFeenAdapter(size_t max_history_size = 10)
        : max_history_size_(max_history_size) {}
#endif

    double compute_battery_trust_from_feen(double current_voltage) {
        if (!std::isfinite(current_voltage)) {
            return 1.0;
        }

#if DS_ENABLE_FEEN
        try {
            // Dummy peers logic (e.g., cell voltages could be used here if available,
            // but we only have a single pack voltage in DSState)
            std::vector<double> peers = {current_voltage};

            feen::ailee::ConfidenceResult result = scorer_.evaluate(
                current_voltage,
                peers,
                voltage_history_
            );

            // Update history
            voltage_history_.push_back(current_voltage);
            if (voltage_history_.size() > max_history_size_) {
                voltage_history_.erase(voltage_history_.begin());
            }

            return result.score;
        } catch (...) {
            // Return safe default on any error
            return 1.0;
        }
#else
        voltage_history_.push_back(current_voltage);
        if (voltage_history_.size() > max_history_size_) {
            voltage_history_.erase(voltage_history_.begin());
        }
        return 1.0;
#endif
    }

    void reset() {
        voltage_history_.clear();
    }
};

} // namespace ds

#endif // BATTERY_FEEN_ADAPTER_HPP
