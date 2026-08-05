# DS EV Battery Enhancement - Diagnostics Reference

This document describes the design, rules, thresholds, and execution of the Pre-Flight Diagnostics layer of the Dual-State (DS) EV Battery Enhancement Software.

---

## 🔍 Pre-Flight Diagnostics Overview

Before deploying, updating, or performing maintenance on the high-performance DS core, the vehicle environment must satisfy rigorous safety criteria. Diagnostics run dynamically using `scripts/diagnostics.py` (which is fully integrated into the top-level `Makefile`).

### Safety Gates
If **any** diagnostic check fails, the installer **refuses** to proceed, and the deployment is **BLOCKED**. This is a safety-critical hard gate to ensure the enhancement software is never loaded under unsafe physical, OS, or firmware conditions.

---

## ⚙️ Standard Diagnostic Metrics & Thresholds

The diagnostics layer reads vehicle telemetry from `/tmp/vehicle_status.json`, `config/vehicle_status.json`, or CLI/environment overrides. It enforces the following default safety limits:

### 1. State of Health (SOH)
* **Threshold**: **SOH ≥ 80.0%**
* **Reasoning**: Batteries below 80% SOH exhibit non-linear chemical degradation, unstable internal resistances, and potential cell degradation that violates the coupling assumptions of the DS physics engine. Enabling enhancement on such cells is blocked to prevent accelerated wear or localized cell overheating.

### 2. State of Charge (SOC)
* **Threshold**: **20.0% ≤ SOC ≤ 90.0%**
* **Reasoning**: To prevent cell-level stress and voltage spikes during flashing and initial calibration, installation is constrained to mid-range charge states. Loading the module at extreme low or high SOC is prohibited to protect cell chemistry.

### 3. Temperature Window
* **Threshold**: **0.0°C ≤ Temperature ≤ 45.0°C**
* **Reasoning**: Sub-zero temperatures increase lithium plating risk under high power current. Above 45°C, batteries are close to soft thermal thresholds and can face safety risks during firmware calibration.

### 4. Dynamic Nominal Voltage
* **Threshold**: Must fall within the vehicle model's specific limits defined in `config/vehicle_profiles.json`.
* **Example Ranges**:
  * `Tesla_Model_3_LR`: 300.0 V to 410.0 V
  * `Tesla_Model_3_SR`: 280.0 V to 360.0 V
  * `Tesla_Cybertruck`: 680.0 V to 920.0 V
  * `Hyundai_Ioniq_5`: 580.0 V to 800.0 V

---

## 🛠️ Diagnostics Commands & Overrides

Developers and automated test rigs can simulate and override diagnostics via CLI flags and environment variables.

### 1. Command Line Overrides
```bash
# Evaluate diagnostics with an overridden battery state (will fail due to low SOH)
./scripts/diagnostics.py --soh 78.5

# Test high temperature scenario (will fail)
./scripts/diagnostics.py --temp 48.0

# Set model specifically
./scripts/diagnostics.py --model Tesla_Cybertruck --voltage 820.0
```

### 2. Environment Variable Overrides
```bash
export DS_SOH=85.0
export DS_TEMP_C=25.0
export DS_SOC=60.0
export DS_VOLTAGE_V=360.0
export DS_MODEL=Tesla_Model_3_LR
export DS_FIRMWARE=v2024.12.15

make diagnostics
```

---

## 📝 Exit Code Matrix

The `scripts/diagnostics.py` script exits with specific integer codes indicating the classification of any failures:

| Exit Code | Meaning | Reason |
| :---: | :--- | :--- |
| **0** | **SUCCESS** | All checks passed; safe to proceed with install/update. |
| **1** | **OS / ENVIRONMENT FAILURE** | Non-compliant OS or Python version < 3.8. |
| **2** | **UNSUPPORTED MODEL** | The vehicle model is not found in `vehicle_profiles.json`. |
| **3** | **UNSUPPORTED FIRMWARE** | The detected firmware version does not match pattern rules. |
| **4** | **SAFETY VIOLATION** | SOH, Temperature, SOC, or Voltage outside of safe ranges. |

All diagnostic actions and results are logged securely to:
`/opt/ds_enhancement/logs/install.log`
