#ifndef DS_WNN_TELEMETRY_BRIDGE_HPP
#define DS_WNN_TELEMETRY_BRIDGE_HPP

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "ds_battery_core.hpp"
#include "ds_bms_middleware_v2.hpp"

namespace ds_wnn {

#pragma pack(push, 1)
/**
 * @brief Tightly packed struct containing the dual-state thermodynamic
 *        telemetry for WNN phase-modulation.
 *
 * Uses `long double` to maintain mathematical determinism with WNN's
 * Kahan summation algorithm.
 */
struct WNNThermodynamicPayload {
    // Physical state variables (Ψ)
    long double voltage;           // Volts
    long double current;           // Amperes
    long double temperature;       // Celsius
    long double state_of_charge;   // 0.0 to 1.0

    // Informational state variables (Φ)
    long double entropy_accumulation; // Normalized entropy
    long double metric_trace;         // g^eff_μν trace
    long double ds_confidence;       // Confidence score
};
#pragma pack(pop)

/**
 * @brief Lock-free, single-producer single-consumer (SPSC) ring buffer queue.
 *
 * Provides thread-safe, wait-free operations to pass telemetry without
 * blocking the WNN hot path.
 *
 * @tparam T The payload type
 * @tparam Capacity The capacity of the ring buffer, must be a power of 2
 */
template <typename T, std::size_t Capacity = 1024>
class SPSCQueue {
    static_assert((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0),
                  "Capacity must be a non-zero power of 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SPSCQueue<T>: T must be trivially copyable for lock-free safety");

public:
    SPSCQueue() : head_(0), tail_(0) {}

    ~SPSCQueue() = default;

    // Non-copyable and non-movable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    /**
     * @brief Pushes an item into the queue.
     * @param item The item to push.
     * @return true if pushed successfully, false if the queue is full.
     */
    [[nodiscard]] bool push(const T& item) {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        std::size_t next_tail = (tail + 1) & mask_;

        // If next_tail == head, queue is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pops an item from the queue.
     * @param item The reference to store the popped item.
     * @return true if popped successfully, false if the queue is empty.
     */
    [[nodiscard]] bool pop(T& item) {
        std::size_t head = head_.load(std::memory_order_relaxed);

        // If head == tail, queue is empty
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        item = buffer_[head];
        head_.store((head + 1) & mask_, std::memory_order_release);
        return true;
    }

private:
    T buffer_[Capacity];
    static constexpr std::size_t mask_ = Capacity - 1;

    // Pad to prevent false sharing between producer and consumer indices
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
};

/**
 * @brief Standalone adapter class bridging DS Battery Middleware to WNN.
 *
 * Serializes thermodynamic data into a lock-free queue, allowing the
 * WNN daemon to read without being blocked by BMS routines.
 */
class DSWNNBridge {
public:
    DSWNNBridge() = default;
    ~DSWNNBridge() = default;

    // Non-copyable and non-movable: owns a lock-free queue with producer/consumer state
    DSWNNBridge(const DSWNNBridge&) = delete;
    DSWNNBridge& operator=(const DSWNNBridge&) = delete;
    DSWNNBridge(DSWNNBridge&&) = delete;
    DSWNNBridge& operator=(DSWNNBridge&&) = delete;

    /**
     * @brief Ingests an EnhancedState and DiagnosticReport from DSBMSMiddleware,
     *        translates them to WNNThermodynamicPayload, and pushes to queue.
     *
     * @param state The current enhanced state from the BMS.
     * @param diag The diagnostic report from the BMS.
     * @return true if pushed to the queue successfully, false if full.
     */
    [[nodiscard]] bool update_telemetry(const ds::EnhancedState& state,
                          const ds_plugin::DiagnosticReport& diag) {
        WNNThermodynamicPayload payload{};

        // Extract Physical State Ψ
        payload.voltage = static_cast<long double>(state.state.voltage);
        payload.current = static_cast<long double>(state.state.current);
        payload.temperature = static_cast<long double>(state.state.temperature);
        payload.state_of_charge = static_cast<long double>(state.state.state_of_charge);

        // Extract Informational State Φ
        // Note: Using DSState's entropy directly, and diag's metrics for the rest.
        payload.entropy_accumulation = static_cast<long double>(state.state.entropy);
        payload.metric_trace = static_cast<long double>(diag.ds_metric_trace);
        payload.ds_confidence = static_cast<long double>(diag.ds_confidence);

        return queue_.push(payload);
    }

    /**
     * @brief Allows the external WNN daemon to safely pop the latest
     *        thermodynamic state off the queue for phase-modulation.
     *
     * @param payload The reference to store the popped payload.
     * @return true if a payload was retrieved, false if the queue is empty.
     */
    [[nodiscard]] bool consume_payload(WNNThermodynamicPayload& payload) {
        return queue_.pop(payload);
    }

private:
    // Capacity 1024 to provide ample buffer size while remaining lock-free
    SPSCQueue<WNNThermodynamicPayload, 1024> queue_;
};

} // namespace ds_wnn

#endif // DS_WNN_TELEMETRY_BRIDGE_HPP
