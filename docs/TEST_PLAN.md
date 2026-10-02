# Test plan

## Primary acceptance case

**Six M8010(B)E17B50L joints, one 1 Mbit/s Classical CANopen bus, RPDO4 + SYNC, 500 Hz.**

Record:

- busiest-bus wire load
- command-to-SYNC latency
- maximum and p99 command latency
- execution skew
- feedback latency
- command and feedback deadline misses
- maximum queued frames
- retransmissions in the stress case

## Comparison cases

1. 250 / 500 / 750 / 1000 Hz on one bus.
2. Same rates with 3+3 motors on two buses.
3. Exact payload-dependent stuffing vs conservative stuffing bound.
4. Asynchronous RPDO1 position mode to quantify the cost of losing synchronized execution.
5. 500 kbit/s sensitivity using `--bitrate 500000`.
6. Firmware-response-delay sensitivity using `--response-delay-us 50`, `100`, `250`.

## Interpretation

A useful architecture should have zero command deadline misses **and** meaningful spare capacity. A result that barely fits nominally is not adequate evidence for a welding robot because retransmissions, diagnostics, MCU scheduling, and physical-layer disturbances consume margin.

The program's screening thresholds are intentionally conservative engineering heuristics:

- `PASS`: no command misses and healthy timing/load margin
- `CAUTION`: no misses but margin is becoming limited
- `MARGINAL`: no misses in the run but load/latency leaves little robustness
- `FAIL`: command deadline misses or essentially saturated/overloaded bus

These labels are not a standard or manufacturer guarantee.
