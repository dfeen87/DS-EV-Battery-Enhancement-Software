# DS BMS – OEM Quick Start Guide

**Audience:** OEMs, Tier-1 suppliers, BMS / ECU integration teams  
**Goal:** Bring DS BMS Middleware online with real hardware as quickly and safely as possible

---

## 1. What You Are Integrating

The DS BMS Middleware is a **physics-informed enhancement layer** that sits
*above* hardware drivers and *below* vehicle control logic.

It does **not** replace:
- Your certified safety controller
- Your contactor interlock logic
- Your inverter control firmware

It **does** provide:
- Advanced state estimation
- Degradation awareness
- Weak-cell and imbalance detection
- Predictive diagnostics
- Deterministic safety gating

---

## 2. Minimal Integration Checklist (15–30 minutes)

To run DS middleware on real hardware, you must implement **only one thing**:

### ✅ A hardware adapter that supplies pack telemetry

Specifically, you must provide implementations for:

- `IPackSensors`
  - Pack voltage (V)
  - Pack current (A, signed)
  - Pack temperature (°C)
  - State of charge (% or normalized)

Optionally:
- `ICellSensors` (for cell-level telemetry)
- `IBatteryActuators` (for contactors / balancing)
- `ITimeSource` (for system time)

A production-lean reference adapter is provided:

`src/ds_bms_hardware_adapter.hpp`

Most OEMs adapt this file rather than starting from scratch.

---

## 3. CAN Integration (Typical Path)

### Step 1 – Map your DBC
Edit `OEMSignalMap` in the hardware adapter:
- Replace CAN IDs with your DBC values
- Verify scaling and sign conventions
- Confirm endianness

Reference: `docs/oem_can_mapping.md`

---

### Step 2 – Implement ICANTransport
Provide a thin wrapper around your existing CAN stack:
- SocketCAN (Linux)
- Vector / Kvaser / PEAK
- Embedded RTOS driver

Only two functions are required:
```cpp
bool send(const CANFrame&);
std::optional<CANFrame> receive();
```

### Step 3 – Poll and Update

In your ECU/BMS main loop (or CAN thread):

```cpp
adapter.set_now_seconds(t);
adapter.poll_can();
adapter.refresh_cells_from_backend(); // if applicable
```

---

## 4. Connecting to the Middleware

Once telemetry is available, integration is straightforward:

```cpp
auto voltage = adapter.read_pack_voltage();
auto current = adapter.read_pack_current();
auto temp    = adapter.read_pack_temperature();
auto soc     = adapter.estimate_soc();

auto enhanced = bms.enhance_cycle(
    voltage, current, temp, soc, dt
);
```

Diagnostics are always available:

```cpp
const auto& diag = bms.get_diagnostics();
```

---

## 5. Safety Expectations

The middleware enforces fail-closed behavior:

- Missing signals → fault
- Stale signals → fault
- Out-of-range values → fault

On fault:
- Control outputs should be derated or disabled
- Higher-level safety controllers take precedence

DS middleware never overrides OEM safety logic.

---

## 6. Optional Enhancements (Later)

After basic integration, OEMs may enable:

- Multi-cell monitoring
- Weak-cell detection
- Voltage imbalance alerts
- Health forecasting
- Fleet telemetry
- Kalman-filtered state estimation

All are optional and require no API changes.

---

## 7. Validation Before Deployment

Before field use, OEMs should verify:

- [ ] Signal freshness under load
- [ ] Fault behavior on signal loss
- [ ] Scaling correctness
- [ ] Temperature edge cases
- [ ] Degraded pack scenarios
- [ ] Safe interaction with torque / power limits

---

## 8. Where to Look Next

| Resource | Location |
|---|---|
| Architecture overview | `docs/architecture_overview.md` |
| CAN mapping reference | `docs/oem_can_mapping.md` |
| Simple pack example | `examples/simple_bms_loop.cpp` |
| Multi-cell pack example | `examples/multicell_pack_loop.cpp` |

---

## 9. Support & Contact

For integration questions or collaboration:

Don Michael Feeney Jr.  
📧 dfeen87@gmail.com
