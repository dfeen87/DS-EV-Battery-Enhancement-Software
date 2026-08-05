# DS BMS Middleware – OEM CAN Mapping Reference

**Version:** 1.0  
**Applies to:** `DSHardwareAdapter`
**Audience:** OEMs, Tier-1 suppliers, BMS / ECU integrators

---

## 1. Purpose

This document defines a **reference CAN signal mapping** used by the
DS BMS Middleware hardware adapter.

It is **not a mandated DBC**.

Instead, it serves as:
- A clear integration template
- A safety reference
- A starting point for OEM-specific adaptation

OEMs are expected to:
- Map these signals to their existing DBC
- Adjust scaling and IDs as needed
- Preserve semantics and units

---

## 2. Design Principles

- **Safety first**: stale data detection is mandatory
- **Conservative defaults**: middleware fails closed
- **Minimal assumptions**: no vendor-specific encodings
- **Readable diffs**: stable signal names

---

## 3. Pack-Level Telemetry Signals (Required)

These signals **must** be provided for DS middleware operation.

| Signal Name        | Description                    | Units | Direction | Required |
|--------------------|--------------------------------|-------|-----------|----------|
| PackVoltage        | Total pack voltage              | V     | HW → ECU  | Yes      |
| PackCurrent        | Pack current (signed)           | A     | HW → ECU  | Yes      |
| PackTemperature    | Representative pack temperature | °C    | HW → ECU  | Yes      |
| PackSOC            | Estimated state of charge       | %     | HW → ECU  | Yes      |

---

## 4. Reference CAN IDs (Example Only)

> ⚠️ **These IDs are placeholders.**  
> OEMs must replace them with values from their DBC.

| Signal          | CAN ID (hex) | Frame Type |
|-----------------|--------------|------------|
| PackVoltage     | 0x180        | Standard   |
| PackCurrent     | 0x181        | Standard   |
| PackTemperature | 0x182        | Standard   |
| PackSOC         | 0x183        | Standard   |

---

## 5. Reference Signal Encoding

### 5.1 Pack Voltage

voltage_V = raw * 0.1

- **Range:** 0 – 6553.5 V

---

### 5.2 Pack Current

- **Type:** Signed 16-bit
- **Endianness:** Little-endian
- **Scaling:**  

current_A = raw * 0.1

- **Sign Convention:**  
- Positive → discharge  
- Negative → charge

---

### 5.3 Pack Temperature

- **Type:** Signed 16-bit
- **Endianness:** Little-endian
- **Scaling:**  

temperature_C = raw * 0.1


---

### 5.4 State of Charge (SOC)

- **Type:** Unsigned 16-bit
- **Scaling:**  

soc_percent = raw * 0.1

- **Valid Range:** 0 – 100 %

---

## 6. Signal Freshness Requirements

To ensure safety and determinism, all required signals must meet freshness
constraints.

| Parameter                | Default |
|--------------------------|---------|
| Maximum signal age       | 500 ms  |
| Missing signal behavior  | Fault   |
| Stale signal behavior    | Fault   |

If a required signal exceeds the freshness threshold, the middleware will:
- Flag a safety fault
- Reject further control outputs
- Require operator or supervisory recovery

---

## 7. Cell-Level Telemetry (Optional)

Cell telemetry may be supplied via:

### Option A – CAN (high bandwidth systems)
- OEM streams cell voltages / temperatures on CAN
- Adapter decodes frames directly

### Option B – Local telemetry (common)
- ECU reads cell monitor ICs via SPI / I2C
- OEM implements `ICellTelemetryBackend`

DS middleware **does not require** cell telemetry, but enabling it allows:
- Weak cell detection
- Voltage imbalance monitoring
- Predictive degradation alerts

---

## 8. Actuator Command Frames

### 8.1 Contactor Control

| Function      | Description                     |
|--------------|---------------------------------|
| DischargeCmd | Enable / disable discharge path |
| ChargeCmd    | Enable / disable charge path    |

**Reference Encoding:**

| Byte | Meaning            |
|------|--------------------|
| 0    | Discharge Enable (0/1) |
| 1    | Charge Enable (0/1)    |

**Reference CAN ID:** `0x200`

> ⚠️ OEMs must integrate this with their own safety interlocks.

---

### 8.2 Cell Balancing Command

Balancing is requested via a **bitmask** representing cell IDs.

- Bit `n` set → balance cell `n`
- Supports up to 64 cells per frame (extendable)

**Reference CAN ID:** `0x201`

OEMs may replace this with:
- Per-cell commands
- Grouped balancing zones
- Internal BMS-only balancing logic

---

## 9. Error Handling Expectations

The adapter is expected to:

- Reject missing or stale signals
- Clamp out-of-range values
- Fail closed on decode errors
- Log faults for traceability

DS middleware **never assumes hardware correctness**.

---

## 10. OEM Integration Checklist

Before deployment:

- [ ] CAN IDs mapped to OEM DBC
- [ ] Scaling verified against hardware
- [ ] Signal freshness validated
- [ ] Safety interlocks tested
- [ ] Cell telemetry path verified (if used)
- [ ] Fault injection tested

---

## 11. Summary

This mapping provides a **safe, minimal, and portable contract** between
DS middleware and real-world battery hardware.

OEMs are encouraged to:
- Keep semantics intact
- Document deviations
- Maintain conservative defaults

---

**Questions / Integration Support:**  
Contact: `dfeen87@gmail.com`

