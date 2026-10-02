# Test 3 — Random CAN Error and Retransmission Sensitivity

## Purpose

Evaluate whether the proposed **six-M8010 / one-bus / 1 Mbit/s / 500 Hz / RPDO4 + SYNC** architecture remains usable when CAN frames occasionally fail and must be retransmitted.

This matters because the robot is intended for an industrial welding environment, where EMI/noise is a realistic design concern. The test is a communication stress model only; it is not a physical electromagnetic-compatibility model.

## Test configuration

- 6 ATAR/Avatar M8010 CANopen nodes
- 1 Classical CAN 2.0A bus
- 1 Mbit/s
- RPDO4 + CANopen SYNC synchronized position path
- 500 Hz / 2 ms control period
- assumed motor response delay: 250 us
- 100 ms heartbeat traffic
- 250 ms periodic SDO traffic
- one EMCY event
- automatic retransmission after a failed CAN transmission attempt
- 5000 control cycles per run
- 5 deterministic random seeds per error probability

The injected error probability is applied independently to each transmission attempt. This is intentionally harsher than a normal healthy CAN installation at the upper test points.

## Results

| Per-attempt error probability | Avg. retries | Max offered load | Max command latency | Max feedback latency | Command misses | Feedback misses | Interpretation |
|---:|---:|---:|---:|---:|---:|---:|---|
| 0% | 0 | 58.7% | 1085 us | 1400 us | 0 | 0 | PASS |
| 0.01% | 6 | 58.7% | 1087 us | 1402 us | 0 | 0 | PASS |
| 0.05% | 33 | 58.7% | 1089 us | 1444 us | 0 | 0 | PASS |
| 0.1% | 66.6 | 58.8% | 1370 us | 1669 us | 0 | 0 | PASS |
| 0.5% | 329.8 | 59.0% | 1202 us | 1517 us | 0 | 0 | PASS |
| 1% | 653.2 | 59.4% | 1213 us | 1638 us | 0 | 0 | PASS |
| 2% | 1325.8 | 60.2% | 1490 us | 1806 us | 0 | 0 | PASS |
| 5% | 3471 | 62.4% | 1606 us | 1934 us | 0 | 0 | CAUTION — very little feedback margin remains |
| 10% | 7335.2 | 66.6% | 1916 us | 2269 us | 0 | 8 | FAIL — feedback deadline violated |

## Interpretation

The 500 Hz one-bus architecture is surprisingly tolerant of isolated random retransmissions because its nominal cyclic load is only about 59%. Even with a deliberately extreme **2% failed-attempt probability**, all five seeded runs completed with zero command and feedback deadline misses.

At **5%**, the system still met all 2 ms deadlines in the sampled runs, but worst feedback latency reached **1.934 ms**, leaving only about **66 us** of timing margin. This should not be considered a comfortable operating point.

At **10%**, feedback latency exceeded the 2 ms control period and feedback deadline misses appeared. Therefore the architecture should not be considered robust under that error intensity.

## Important caveat

Independent random errors are not the only relevant failure pattern for welding. A real disturbance may create a **short correlated burst** of multiple consecutive frame failures. A burst can be more damaging to one control cycle than the same number of errors spread over several seconds.

Therefore Test 3 is split into:

1. **Test 3A — random independent errors** — documented here.
2. **Test 3B — deterministic burst errors** — forces 1, 2, 3, 5, 10, 20, and 50 consecutive failed transmission attempts near the middle of a run.

## Procurement/control implication

The result supports the conclusion that **normal occasional CAN retransmissions are not a reason to reject the M8010 communication architecture** at 500 Hz.

However, the physical robot must still use proper CAN wiring, termination, shielding/grounding, routing away from welding-current paths, and EMC-conscious installation. This simulation cannot substitute for hardware EMC testing.

For future admittance control, feedback deadline integrity is treated as a first-class requirement. The simulator screening has therefore been tightened so that **any feedback deadline miss is a FAIL** rather than being hidden by a command-only result.
