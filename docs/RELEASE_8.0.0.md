# DS-EV 8.0.0 Engineering and Release Report

## Scope and safety boundary

Version 8.0.0 strengthens the execution contract around the existing Dual-State
battery model. It does not replace the DS mathematics, change subsystem ownership,
or grant AILEE control authority. Passing software or simulation tests is **not**
hardware validation, vehicle-specific validation, or real-world EV safety
certification.

## 1. Invariants identified

| Boundary | Required invariant | Deterministic failure |
|---|---|---|
| `DSConfig` | Every scalar is finite; capacity, voltage, current limits and time constants are positive; ranges are ordered | Initialization rejects the configuration |
| `DSState` | All physical, informational, gradient, energy and time values are finite; SOC, entropy and degradation are bounded | Update rejects the candidate state |
| BMS cycle | Voltage and `dt` are positive; every sensor and `dt` is finite | Exception before state mutation; last-known-good state remains observable |
| Energy accounting | `energy_total` equals the sum of the three owned energy terms | Nonzero residual marks numerical instability |
| Trust evidence | Scores are finite and in `[0,1]`; governance level is known; evidence exists | Level 3, 25% envelope, learning disabled |
| Learning | Trust and envelope values are finite, ordered and step-bounded | Proposed parameter is not applied |
| Torque | Command/context fields are finite and in documented domains | Zero torque and Level 3 |
| Compartments | Sensor validity and physical domains are explicit | Zero trust and Level 3 |

State updates are transactional: sensor normalization and DS mathematics execute on
a candidate copy, which is committed only after all postconditions pass. This avoids
a failed cycle exposing a mixture of old informational state and new physical state.

## 2. Previously untested invariants

Tests previously covered ordinary cycles, threshold levels, learning steps, rollback,
and an explicit sensor-valid flag. They did not cover NaN/infinity in configuration,
SOC or trust evidence; non-positive `dt`; atomicity after rejected input; malformed
parameter envelopes; or malformed torque commands.

## 3. Confirmed defects

* NaN bypassed ordered comparisons in configuration, state, and trust checks.
* A NaN trust score could leave minimum trust at `1.0`, authorizing Level 0.
* Invalid `dt` could silently skip or poison time/state calculations.
* `DSEnhancement::enhance` wrote raw physical fields before validation, so failure
  could leave partially mutated state.
* The energy check compared consecutive stored-energy snapshots and mislabeled
  legitimate sensor-driven energy changes as conservation failures.
* CI compiled tests but executed only an example.
* Release builds defined `NDEBUG`, compiling every assertion out of the C++ test
  executables even if CTest had been invoked.

## 4. Risks investigated but not confirmed

The multi-cell, RAPS, regen slew, middleware and Python governance paths were reviewed
for boundary behavior. Their architecture remains unchanged. Concurrency in the
learning engine is mutex-protected, but the battery enhancement and control managers
remain per-loop objects and are not advertised as concurrently callable. Real sensor
freshness, clock synchronization, hardware isolation, CAN integrity, chemistry model
accuracy, and actuator response cannot be established in repository tests.

The Python-embedded governor remains available as an explicit build option. The
deterministic C++ governor is the dependency-free default, avoiding network/toolchain
availability silently changing which trust path is built.

## 5. Regression tests added

The core suite now attacks non-finite configuration and sensor values, invalid time
steps, and verifies rejected cycles preserve last-known-good state. The trust suite
attacks non-finite evidence, recovery trust, learning proposals, BMS telemetry and
torque commands, verifying deterministic Level 3/zero-actuation outcomes.

## 6. Implementation changes

Validation is exhaustive over owned scalar state. BMS updates use candidate/commit
semantics. Energy conservation reports an accounting residual. Trust decisions expose
`evidence_valid` and `evidence_count`, giving downstream audit code an explicit proof
of whether the evidence set passed schema/domain validation. Torque commands and
compartment inputs now fail closed before numerical governance.

## 7. AILEE v9.4 inspection and provenance

The requested upstream repository is `dfeen87/AILEE-Trust-Layer`. During this release
pass, its public repository page identified its current stable release as v5.0.0 and
exposed the canonical pipeline concepts (confidence thresholds, GRACE mediation,
peer consensus, deterministic fallback, audit metadata, and the unified client).
Direct source/tag retrieval was blocked by the execution environment, and no public
v9.4 tag or source could be verified. Therefore this repository does **not** claim
that copied or guessed v9.4 code is present. The integration is explicitly named the
**AILEE Trust Contract 9.4 target** and implements only semantics verified locally or
from the public upstream description: bounded evidence, minimum-trust aggregation,
deterministic levels, fail-closed fallback, bounded learning, snapshots and rollback.
A release must not assert binary/API conformance to an upstream v9.4 artifact until
that artifact is supplied and its implementation and tests can be pinned and audited.

## 8. AILEE functionality connected

The boundary is now explicit:

`physical sensors → DS mathematics on candidate state → validated state → AILEE evidence validation/minimum-trust governance → authorized or restricted torque/regen/BMS decision → evidence metadata and audit snapshot`.

Unknown, empty or malformed evidence never becomes permission. Parameter adaptation
continues to require trust `>= 0.85`, an ordered envelope and a bounded step. Snapshot
rollback remains owned by the learning engine.

## 9. Behavior intentionally unchanged

DS remains the battery mathematics owner. AILEE remains governance rather than a
battery model or actuator. Existing four governance levels, derate factors, threshold
policy, learning threshold, rollback model, RAPS membrane, torque/regen ownership and
public DS concepts remain intact.

## 10. CI enforcement

CI now runs every registered CTest executable and both Python unittest suites before
the simulation smoke test. Test targets explicitly keep assertions enabled in Release
builds. Building tests—or executing assertion-free binaries—is no longer enough.

## 11. Version declarations

CMake/CPack, the C++ API/CLI source of truth, README badge, installer audit log,
architecture/simulation documents, RAPS release marker and update payload fallback
identify DS-EV 8.0.0. Historical milestone and independently versioned module/API
references remain unchanged.

## 12. Remaining architectural risks

* Trust audit files are append-only text, not cryptographically authenticated.
* Evidence freshness and provenance identity are supplied by integrators, not proven
  by the current in-process structures.
* Several control objects assume single-threaded loop ownership.
* Empirical constants require chemistry-, pack-, inverter- and vehicle-specific
  calibration; repository defaults are not deployment approval.
* An authoritative, obtainable AILEE v9.4 source pin is still required for verified
  upstream conformance.

## 13. Validation outside software tests

Required stages remain separate: software correctness, simulation validation,
integration/SIL validation, HIL and battery-pack validation, vehicle-specific
calibration and fault injection, closed-course validation, and applicable independent
real-world safety certification. AILEE authorization and a green CI run satisfy none
of the later stages by themselves.
