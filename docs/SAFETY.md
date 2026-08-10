# DS EV Battery Enhancement - Safety & Disclaimers

Dual-State (DS) Battery Enhancement operates at the intersection of mathematical field physics and advanced chemistry state estimation. To ensure optimal performance and avoid any hazard, the system enforces rigid, deterministic safety boundaries and fail-closed behaviors.

---

## 🛑 Important Disclaimers

### 1. Intended Use
This enhancement system is designed to run in parallel with standard automotive BMS micro-controllers. It is an **information-physical software optimizer** and **does not bypass** any hard safety mechanisms programmed into your vehicle's original equipment manufacturer (OEM) controller or battery contactors.

### 2. Liability Limit
This software is provided "as is" under the MIT License terms. While it implements highly verified physical constraints (including thermodynamic Kahan summation and Thermodynamic energy bounds), users and vehicle engineers assume all risks associated with deploying aftermarket firmware configurations on production electric vehicles.

---

## 🛡️ Operational Safety Boundaries

The DS core engine establishes three layers of protection to ensure battery safety:

### 1. State of Health (SOH) Thresholds
* **Limit**: Battery State of Health must be **>= 80%** to qualify for core state-coupling enhancements.
* **Reasoning**: Heavily degraded batteries (SOH < 80%) exhibit chaotic internal physical metrics ($\Psi$) that can violate the linear assumptions of the effective metric coupling model ($g^{\text{eff}}_{\mu\nu}$).
* **Action**: If SOH is below 80%, the installation or execution is blocked, and the system defaults to pass-through non-coupling protection mode.

### 2. State of Charge (SOC) Bounds
* **Minimum Threshold**: **20%**
* **Maximum Threshold**: **90%**
* **Reasoning**: Restricting installation/flashing/calibration to mid-range SOC prevents electrochemical cell stress and avoids accidental deep-discharge or over-voltage scenarios during deployment.

### 3. Temperature Envelope
* **Operating Range**: **0°C to 45°C**
* **Reasoning**: At sub-zero temperatures, lithium plating risk increases dramatically, altering the informational gradient ($\partial_{\mu}\Phi$). At temperatures above 45°C, thermal runaways or advanced chemical decomposition can trigger.
* **Action**: Diagnostics will immediately fail and block installation if the cell temperatures are outside this window.

### 4. Dynamic Voltage Boundaries
* **Nominal Pack Window**: Checked dynamically against the vehicle's model profile in `vehicle_profiles.json` (e.g. 300V to 410V for typical NMC battery packs).
* **Action**: Any voltage reading exceeding the specific profile's limits triggers a voltage fault, disabling high-power charging or aggressive regenerative blending.

---

## 🔒 Fail-Closed & Signal Freshness Philosophy

If any critical telemetry signal (such as voltage, current, or temperature) is delayed, corrupted, or missing, the DS stack executes a **strict fail-closed protocol**:

1. **Information Decay**: The informational state Φ immediately decays back to ground state ($\Phi \to 0$) with zero coupling effect.
2. **Metric Trace Convergence**: The effective metric $g^{\text{eff}}_{\mu\nu}$ relaxes back to flat baseline space $g_{\mu\nu}$, disabling any predictive power enhancements.
3. **Control Authority Handover**: All dynamic control scaling factors (torque fractions, regen bounds) return to standard OEM limits or are bypassed entirely.
4. **Independent Protection**: Original vehicle hard-limits and contactor safety logic remain fully active and unaffected.

---

## 🚨 Emergency Limp-Home & Performance Derating

The DS system defines and enforces explicit derating states under critical vehicle conditions:

### 1. Critical SOC (Limp-Home Mode)
* **Trigger**: State of Charge falls below **5.0%** (`soc < config.battery.soc_min_critical`).
* **Behavior**:
  - The DS Torque Manager immediately activates `limp_mode_active = True`.
  - Maximum available motor torque is limited to **20%** of peak rating to protect the pack from deep discharge and cell reversal.
  - Diagnostics triggers visual dashboard warnings.

### 2. High Temperature Derating
* **Trigger**: Pack or component temperature exceeds **45.0°C** (`temp_soft_limit_c`).
* **Behavior**:
  - The Torque Manager activates `thermal_derate_active = True`.
  - Available torque is progressively and dynamically scaled down (up to **60% reduction**) as temperatures approach 60°C.
  - If temperature exceeds 60°C, the system triggers an emergency thermal shutdown to prevent thermal runaway.

### 3. Weak-Cell Detection
* **Trigger**: Voltage imbalance among cells exceeds **50mV** or any cell exhibits an anomalous SOH degradation rate.
* **Behavior**:
  - Available charging and discharging power limits are reduced (scaled down dynamically based on the number and severity of weak cells).
  - High-power regenerative braking is constrained to prevent weak-cell overvoltage.
  - Cell balancing is commanded immediately.

### 4. Closed-Loop Regenerative Blinking
* If a physical CAN bus warning or stability control (ABS/ESC) event is active:
  - The regenerative braking system fully disengages within **10ms**.
  - 100% braking authority is yielded to the physical mechanical / friction brakes to ensure driving stability.
