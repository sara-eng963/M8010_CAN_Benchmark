# Test 2 — M8010 TPDO Response-Delay Sensitivity at 500 Hz

## Purpose

Evaluate how much unknown internal M8010 firmware delay can be tolerated before the planned **6-axis / one 1 Mbit/s CANopen bus / RPDO4 + SYNC / 500 Hz** architecture begins to return stale feedback or miss the 2 ms control deadline.

This test is especially relevant to the future **F/T-sensor admittance controller**, because the outer controller needs timely joint feedback as well as timely commands.

## Test configuration

- 6 M8010 nodes
- 1 Classical CANopen bus
- 1 Mbit/s
- 500 Hz control rate
- 2 ms control period
- synchronized RPDO4 + SYNC command path
- TPDO4 feedback path
- motor response delay swept from 0 to 2000 us
- nominal, conservative wire-bound, and production-stress traffic profiles

The swept delay is an **engineering sensitivity parameter**. It is not a claimed ATAR specification.

## Results

| Assumed motor response delay | Nominal feedback max | Conservative feedback max | Stress feedback max | Feedback deadline behavior |
|---:|---:|---:|---:|---|
| 0 us | 1.179 ms | 1.315 ms | 1.290 ms | 0 misses |
| 100 us | 1.179 ms | 1.315 ms | 1.323 ms | 0 misses |
| 250 us | 1.293 ms | 1.415 ms | 1.405 ms | 0 misses |
| 500 us | 1.201 ms | 1.315 ms | 1.344 ms | 0 misses |
| 750 us | 1.451 ms | 1.535 ms | 1.564 ms | 0 misses |
| 1000 us | 1.701 ms | 1.785 ms | 1.814 ms | 0 misses |
| 1250 us | 1.951 ms | 2.785 ms | 2.064 ms | nominal: 0; conservative: all; stress: 62 misses |
| 1500 us | 2.870 ms | 2.935 ms | 2.961 ms | all 5000 cycles miss |
| 1750 us | 3.073 ms | 3.205 ms | 3.180 ms | all 5000 cycles miss |
| 2000 us | 3.281 ms | 3.455 ms | 3.380 ms | all 5000 cycles miss |

## Interpretation

### Robust region

Up to an assumed **1000 us internal response delay**, all three traffic profiles completed feedback within the 2 ms period with **zero feedback deadline misses**.

This gives useful evidence that the 500 Hz architecture has meaningful tolerance to nonzero drive processing delay.

### Transition region

At **1250 us**, the system becomes highly sensitive to exact CAN timing and background traffic:

- nominal case still completes just inside the deadline
- conservative wire-bound case misses every cycle
- production-stress case begins to miss cycles

Therefore 1.25 ms should not be treated as a safe firmware delay for this design.

### Failure region

At **1500 us and above**, every 500 Hz cycle misses the feedback deadline in all three profiles.

## Why command latency is not monotonic with motor delay

Increasing TPDO response delay does not necessarily increase command latency monotonically. Delaying TPDO readiness changes when feedback frames compete with RPDO and SYNC traffic on the shared CAN bus. In some cases this moves TPDO traffic away from the command burst and temporarily reduces command contention.

This does **not** mean a slower motor response improves the servo. For admittance-control design, the important quantity in this test is feedback age and feedback deadline misses.

## Engineering conclusion

> **A one-bus 500 Hz architecture remains communication-feasible with substantial modeled firmware delay, but a conservative design should require the cyclic TPDO response path to remain below approximately 1 ms.**

The exact production M8010 firmware should therefore be confirmed with the supplier for:

- maximum RPDO-to-TPDO processing/response delay
- TPDO inhibit time or minimum transmission interval
- whether TPDO4 can be produced every 2 ms continuously
- whether the documented 2 ms interpolation mode is supported with six nodes on the same bus

For future admittance control, <=1 ms response delay is a useful provisional procurement target because it retained zero feedback misses in nominal, conservative, and stress simulations.

## Test artifact

Run:

```bash
bash scripts/run_delay_sweep.sh
```

Raw results:

```text
results/delay_sweep/
```

## Status

**Test 2 result: PASS with condition.**

The network architecture tolerates up to 1 ms assumed motor response delay in the tested 500 Hz configuration. Actual M8010 firmware timing remains a supplier-confirmation item.
