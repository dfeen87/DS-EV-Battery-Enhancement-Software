#ifndef DS_BMS_INTERFACES_HPP
#define DS_BMS_INTERFACES_HPP

#include <vector>
#include <string>

namespace ds_plugin {

/*
 * ============================================================================
 * HARDWARE ABSTRACTION INTERFACES
 * ============================================================================
 * These interfaces decouple DS middleware from:
 *  - CAN bus implementations
 *  - ADC drivers
 *  - Simulators / HIL rigs
 *  - Future vehicle platforms
 *
 * NO LOGIC. CONTRACTS ONLY.
 * ============================================================================
 */

/* ================= TIME SOURCE ================= */

class ITimeSource {
public:
    virtual ~ITimeSource() = default;
    virtual double now_seconds() const = 0;
};

/* ================= PACK-LEVEL SENSORS ================= */

class IPackSensors {
public:
    virtual ~IPackSensors() = default;

    virtual double read_pack_voltage() const = 0;     // Volts
    virtual double read_pack_current() const = 0;     // Amps (positive = discharge)
    virtual double read_pack_temperature() const = 0; // °C
    virtual double estimate_soc() const = 0;           // 0.0 – 1.0
};

/* ================= CELL-LEVEL SENSORS ================= */

class ICellSensors {
public:
    virtual ~ICellSensors() = default;

    virtual std::vector<double> read_cell_voltages() const = 0;
    virtual std::vector<double> read_cell_temperatures() const = 0;
};

/* ================= ACTUATORS ================= */

class IBatteryActuators {
public:
    virtual ~IBatteryActuators() = default;

    virtual void enable_discharge(bool enable) = 0;
    virtual void enable_charge(bool enable) = 0;
    virtual void request_cell_balancing(const std::vector<int>& cell_ids) = 0;
};

/* ================= OPTIONAL LOGGER ================= */

class IBMSLogger {
public:
    virtual ~IBMSLogger() = default;
    virtual void log_event(const std::string& message) = 0;
};

} // namespace ds_plugin

#endif // DS_BMS_INTERFACES_HPP
