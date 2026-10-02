# Test 5 — CAN Bitrate Sensitivity and Dual-Bus Fallback

## Purpose

Determine whether the planned six-axis M8010 architecture depends on **1 Mbit/s CANopen**, and verify whether a **two-bus 3+3 fallback** remains viable if the motors are limited to 500 kbit/s.

## Primary architecture

- 6 M8010 nodes
- one Classical CANopen bus
- RPDO4 + SYNC
- 500 Hz / 2 ms cycle
- 250 us assumed motor TPDO response delay

## One-bus bitrate sweep

| Bitrate | Nominal load | Wire-bound load | Stress load | Deadline result | Interpretation |
|---:|---:|---:|---:|---|---|
| 250 kbit/s | 232.9% | 263.0% | 234.9% | FAIL | Physically over capacity |
| 500 kbit/s | 116.5% | 131.5% | 117.4% | FAIL | Physically over capacity |
| 800 kbit/s | 72.8% | 82.2% | 73.4% | No misses, but low margin | CAUTION / MARGINAL |
| 1 Mbit/s | 58.2% | 65.8% | 58.7% | PASS | Recommended one-bus design point |

## Main conclusion from the one-bus sweep

The one-bus 500 Hz design is strongly dependent on operating at **1 Mbit/s**.

At 500 kbit/s, the offered cyclic traffic exceeds the available wire capacity, so the backlog grows continuously. Therefore written supplier confirmation of 1 Mbit/s support remains a procurement requirement if one shared CAN bus is used for all six joints.

## Two-bus fallback at 500 kbit/s

The STM32H745 provides two CAN/FDCAN peripherals, so a fallback architecture with three motors per bus was checked:

```text
CAN bus A -> joints 1, 2, 3
CAN bus B -> joints 4, 5, 6
```

At 500 kbit/s and 500 Hz:

| Profile | Load on busiest bus | Max command latency | Max cross-bus execution skew | Command misses | Result |
|---|---:|---:|---:|---:|---|
| Nominal | 60.8% | 820 us | 208 us | 0 | PASS |
| Wire bound | 68.5% | 680 us | 0 us in this deterministic ordering | 0 | PASS |
| Production stress | 61.3% | 1222 us | 332 us | 0 | PASS |

## Interpretation

This establishes an important fallback:

> **If 1 Mbit/s is unavailable, 500 kbit/s can still support 500 Hz cyclic control when the six joints are split 3+3 across two buses.**

However, the two-bus architecture is not equivalent to one shared synchronized bus.

Each bus receives its own SYNC frame. Because each bus may be busy with different traffic at that instant, the two SYNC frames can complete at slightly different times. The current model therefore observes up to approximately **208 us nominal** and **332 us under stress** of cross-bus execution skew.

That skew is a synchronization issue, not a raw bandwidth issue.

## Current architecture preference

### Preferred

```text
1 bus
6 joints
1 Mbit/s
500 Hz
RPDO4 + SYNC
```

Advantages:

- simpler network
- one common SYNC frame for all six joints
- modeled inter-axis execution skew is effectively zero on the single bus
- substantial bandwidth margin

### Fallback

```text
2 buses
3 + 3 joints
500 kbit/s per bus
500 Hz
```

Advantages:

- bandwidth is sufficient
- zero command deadline misses in the tested profiles

Additional concern:

- cross-bus SYNC alignment must be verified and controlled

## Procurement implication

Supplier confirmation should include the exact supported bitrate of the M8010(B)E17B50L firmware supplied to the project.

If 1 Mbit/s is supported, the one-bus architecture remains the preferred design.

If only 500 kbit/s is supported, the motor is **not automatically disqualified** from a communication-bandwidth perspective because the STM32H745 dual-bus 3+3 architecture still fits at 500 Hz. The remaining question becomes cross-bus synchronization quality.

## Next test

Test 5B should characterize **dual-bus synchronization skew** across motor response delays and background traffic to determine whether the 3+3 fallback is acceptable for coordinated welding and admittance control.
