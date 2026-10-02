# Test 1 — M8010 Classical CANopen Bandwidth and Cycle-Timing Validation

## Purpose

Validate whether **six ATAR/Avatar M8010(B)E17B50L joints** can share one **1 Mbit/s Classical CANopen bus** with enough deterministic timing margin for the planned 6-DoF welding robot and future F/T-sensor-based admittance control.

This test is specifically about **communication timing and bus capacity**. It does not validate motor torque, mechanical performance, thermal behavior, or admittance stability.

## Relevant M8010 communication model

The benchmark models the M-series CANopen traffic described in the available documentation:

- Classical CAN 2.0A
- 11-bit CAN identifiers
- 1 Mbit/s bus
- maximum 8-byte CAN payload
- six CANopen nodes
- synchronized interpolated position path using **RPDO4 + SYNC**
- RPDO4: `0x500 + Node ID`, 4-byte target position
- CANopen SYNC: `0x080`
- TPDO4: `0x480 + Node ID`, 6-byte actual position + statusword
- comparison against asynchronous RPDO1 position control

The model includes real CAN frame serialization, CRC-15/CAN, payload-dependent bit stuffing, CAN-ID arbitration, shared-bus queueing, and retransmission sensitivity.

## Test matrix

Two bus architectures were checked:

1. **One bus:** 6 motors on one 1 Mbit/s CAN bus
2. **Two buses:** 3 motors per bus

Control frequencies:

- 250 Hz — 4 ms cycle
- 500 Hz — 2 ms cycle
- 750 Hz — 1.333 ms cycle
- 1000 Hz — 1 ms cycle

Three traffic profiles were used:

- **Nominal:** cyclic motor traffic with exact data-dependent CAN stuffing
- **Wire bound:** conservative payload-independent stuffing bound
- **Production stress:** cyclic traffic plus heartbeat, periodic SDO diagnostics, one EMCY event, and injected retransmission/error sensitivity

## Main result — synchronized RPDO4 + SYNC, one bus

| Control rate | Nominal load | Conservative load | Stress load | Max command latency | Deadline misses | Result |
|---|---:|---:|---:|---:|---:|---|
| 250 Hz | 29.1% | 32.9% | 29.6% | ~1.08–1.20 ms | 0 | PASS |
| 500 Hz | 58.2% | 65.8% | 58.7% | ~1.08–1.20 ms | 0 | PASS |
| 750 Hz | 87.4% | 98.6% | 87.8% | ~1.08–1.20 ms | 0 | MARGINAL / FAIL bound |
| 1000 Hz | 116.5% | 131.5% | 117.0% | backlog grows without bound | 5000 | FAIL |

## Important 500 Hz result

For the intended **2 ms / 500 Hz synchronized control cycle**:

- nominal offered load: **58.2%**
- conservative CAN wire bound: **65.8%**
- production-stress load: **58.7%**
- maximum command-to-SYNC latency: approximately **1.08–1.20 ms**
- command deadline misses: **0**
- modeled synchronized execution skew: **0 us**
- approximately **0.8 ms of the 2 ms cycle remains** after the worst modeled command transmission phase

This indicates that the 8-byte Classical CAN payload limit is **not itself a bandwidth blocker** for the intended synchronized M8010 position-control architecture.

## Why SYNC matters

With asynchronous RPDO1 control, each motor applies its target when its individual frame arrives. Because CAN frames are serialized, the six joints do not update at exactly the same time.

At 500 Hz on one bus, the asynchronous test showed approximately **1.1–1.2 ms of inter-axis execution skew**.

With **RPDO4 + SYNC**, each joint stores its new target and waits for the common CANopen SYNC frame. All six joints then apply their targets at the same control boundary in the model. This is much more appropriate for coordinated multi-axis welding motion and for future admittance control.

## Two-bus comparison

Splitting the system into two 1 Mbit/s buses with 3 motors per bus dramatically increases timing margin. In the model, even 1 kHz traffic fits from a raw bandwidth perspective.

However, two buses introduce a new concern: **cross-bus synchronization skew**. Therefore the simpler one-bus architecture remains attractive if 500 Hz meets the final control requirements.

## Engineering interpretation

### Supported design point

**6 motors / 1 Classical CANopen bus / 1 Mbit/s / 500 Hz / RPDO4 + SYNC**

The current communication model supports this as a viable design point with reasonable bandwidth margin.

### Not recommended

- **750 Hz on one bus:** too little robustness margin
- **1 kHz on one bus:** physically over capacity for the modeled traffic
- **asynchronous position control for coordinated motion:** bandwidth may fit, but joint update skew is undesirable

## Current decision

> **CAN communication status: provisionally acceptable for the M8010 at 500 Hz.**
>
> Six joints can share one 1 Mbit/s Classical CANopen bus in the modeled RPDO4 + SYNC architecture without cyclic deadline misses, while retaining substantial bus headroom.

## Remaining uncertainty

This simulation does **not** prove the behavior of the exact production firmware supplied with the motors. Before bulk purchase, written supplier confirmation is still required for the exact M8010(B)E17B50L firmware/EDS regarding:

- 1 Mbit/s Classical CANopen support
- RPDO4 target-position mapping
- synchronous PDO transmission type / SYNC behavior
- documented 2 ms interpolation cycle
- TPDO mapping and minimum response / inhibit times

The largest remaining timing unknown is the motor firmware's internal delay between receiving the cyclic command and making feedback available. This is addressed in **Test 2 — response-delay sensitivity**.

## Test artifact

Baseline CSV:

```text
results/m8010_can_benchmark.csv
```

Benchmark implementation:

```text
src/benchmark/
```
