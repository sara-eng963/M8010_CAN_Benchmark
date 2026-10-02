# Test 4A — Admittance End-to-End Timing Budget

## Purpose

Estimate how much of the 2 ms / 500 Hz control period remains for the **F/T-sensor + filtering + admittance + kinematics/IK computation** before the six M8010 targets are placed on CAN.

This test connects the communication benchmark to the planned future hand-guiding architecture:

```text
F/T sample
  -> filtering / bias handling
  -> 6D admittance law
  -> Cartesian-to-joint mapping / IK
  -> six RPDO4 targets
  -> CANopen SYNC
  -> all six joints execute together
  -> TPDO feedback becomes available
```

## Communication assumptions

- 6 M8010 nodes
- one 1 Mbit/s Classical CANopen bus
- 500 Hz / 2 ms cycle
- RPDO4 + SYNC synchronized position path
- 250 us assumed internal motor TPDO response delay
- nominal, conservative-wire and production-stress communication profiles

## What `preCAN` means

`preCAN` is an aggregate sensitivity variable representing time consumed before the first RPDO is ready:

- F/T acquisition / timestamp handling
- signal filtering
- gravity / bias compensation if used
- 6D admittance calculation
- Jacobian / Cartesian-to-joint mapping
- IK or joint-reference generation
- command preparation on the STM32

It is **not yet a measured STM32 execution time**.

## Method

The existing communication model was run at 500 Hz with a 250 us motor feedback response assumption. The measured worst communication latencies were then combined with a range of assumed pre-CAN processing budgets.

For each profile:

```text
total command age  = preCAN + worst command-to-SYNC latency
total feedback age = preCAN + worst cycle-start-to-TPDO latency
```

The 2 ms cycle was used as a strict same-cycle scheduling deadline.

## Results

| preCAN | Nominal total feedback | Wire-bound total feedback | Stress total feedback | Same-cycle feedback result |
|---:|---:|---:|---:|---|
| 0 us | 1293 us | 1415 us | 1405 us | PASS |
| 100 us | 1393 us | 1515 us | 1505 us | PASS |
| 250 us | 1543 us | 1665 us | 1655 us | PASS |
| 500 us | 1793 us | 1915 us | 1905 us | PASS, low margin |
| 750 us | 2043 us | 2165 us | 2155 us | FAIL |
| 1000 us | 2293 us | 2415 us | 2405 us | FAIL |

Command execution itself still met the 2 ms deadline at 750 us pre-CAN delay, and in the nominal case at 1000 us. The stricter limit came from requiring all modeled feedback to return before the end of the same 2 ms cycle.

## Timing margin

Using the conservative wire-bound profile:

```text
2000 us - 1415 us = 585 us
```

So the strict same-cycle model allows approximately **585 us maximum aggregate pre-CAN processing time**.

At a 500 us pre-CAN budget, only about **85 us** remains in the conservative profile. Therefore 500 us technically passes this test but should not be treated as a comfortable software target.

A practical preliminary implementation target is to keep F/T handling + admittance + kinematics + command preparation materially below 500 us until execution time is measured on the STM32H745.

## Important interpretation for admittance

The 2 ms TPDO requirement used here is intentionally conservative. A sampled-data controller does not necessarily need feedback generated after the current command to return inside that same cycle; many real-time controllers operate as a pipeline and use the latest available state from the previous sample.

Therefore a `feedback FAIL` in this test does **not automatically mean admittance control is impossible**. It means the strict assumption “new sample -> compute -> command -> execute -> all feedback back inside the same 2 ms window” no longer holds.

The more fundamental admittance quantity is the **force-sample-to-synchronized-actuation latency and its jitter**, together with the age of the joint-state feedback actually used by the controller.

## Important limitation

This is an **additive timing-budget sensitivity test**, not a phase-accurate simulation of delayed command release. Delaying RPDO release can change arbitration timing relative to heartbeat/SDO/TPDO traffic.

Also, the public M8010 documentation does not establish exactly when TPDO4 is produced relative to RPDO4 reception and SYNC execution. The benchmark therefore treats TPDO timing as a communication sensitivity model rather than a verified firmware behavior.

## Current conclusion

For the planned 500 Hz architecture, the communication results do not rule out admittance control.

- A 250 us assumed pre-CAN compute budget has substantial margin.
- A 500 us budget still fits the strict same-cycle model, but with little conservative feedback margin.
- 750 us and above no longer satisfy the strict same-cycle feedback assumption.

The next implementation-stage requirement is to benchmark the actual STM32 execution time of the F/T filtering, admittance, Jacobian/IK and command-generation path, and to model the real pipelined control schedule rather than assuming all feedback must return in the same cycle.
