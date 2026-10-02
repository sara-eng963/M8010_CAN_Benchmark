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
  -> TPDO feedback becomes available for the next control cycle
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

The existing communication model is run at 500 Hz with a 250 us motor feedback response assumption. The measured worst communication latencies are then combined with a range of assumed pre-CAN processing budgets.

For each profile:

```text
total command age  = preCAN + worst command-to-SYNC latency
total feedback age = preCAN + worst cycle-start-to-TPDO latency
```

The 2 ms cycle is used as the scheduling deadline.

## Important limitation

This is an **additive timing-budget sensitivity test**, not yet a phase-accurate simulation of delayed command release. Delaying RPDO release can change arbitration timing relative to heartbeat/SDO/TPDO traffic. Test 4B should model the actual task schedule or measure it on the STM32 implementation.

## Decision use

The test answers a practical question before implementing the complete admittance controller:

> How much computation time can the STM32/control software consume while still leaving enough time for synchronized CAN transmission and fresh feedback before the next 2 ms cycle?

For future admittance control, feedback deadline is particularly important because stale joint state increases effective control-loop delay.
