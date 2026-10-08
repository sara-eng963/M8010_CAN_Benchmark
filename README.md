# M8010 Classical CAN Virtual-Time Benchmark

This repository answers one narrow engineering question:

> Given six AVATAR/M8010 nodes using RPDO4 + SYNC + TPDO4 traffic on one 1 Mbit/s Classical CAN bus, can the network itself sustain the requested cyclic communication rate under an ideal deterministic timing model?

The default workload is six nodes at 500 Hz / 2 ms:

- RPDO4: `0x501..0x506`, 4 data bytes each
- SYNC: `0x080`, 0 data bytes
- TPDO4: `0x481..0x486`, 6 data bytes each
- 13 CAN frames per completed cycle

This is a virtual-time CAN timing benchmark. It is not a benchmark of Linux, the PC scheduler, or host execution speed.

## What changed

The original demo acknowledged a frame immediately and delivered it after a fixed 2 ms delay. The configured CAN bitrate therefore did not determine transmission time, and the model could not represent arbitration or serialization.

The benchmark now keeps three quantities separate:

1. drive/application processing delay via `--node-response-us`
2. CAN arbitration / queue delay
3. CAN wire transmission time

The fixed 2 ms frame delay and wall-clock `sleep_for()` are removed.

## CAN timing model

The model is Classical CAN 2.0A with standard 11-bit identifiers.

For every data frame the code builds the transmitted bit sequence from SOF through the 15-bit CRC sequence, computes CAN CRC-15 with polynomial `0x4599`, and applies standard bit stuffing. Stuffing is therefore payload-dependent.

The output distinguishes:

- raw frame bits
- stuff bits
- frame bits on the wire
- 3-bit intermission
- total bus bits
- frame completion time
- bus release time

At 1 Mbit/s, one bit consumes 1 us of virtual time.

The current model assumes an ideal error-free bus, successful ACK, no error frames/retransmissions, no propagation-delay model, and no known physical M8010 processing latency.

## Arbitration and bus occupancy

Only one frame can occupy the network at a time.

When multiple frames are ready at the same virtual timestamp, the lower 11-bit CAN identifier wins. Losing frames remain pending and retry at the next arbitration opportunity. Their original request timestamps are preserved, allowing queue/arbitration delay to be measured.

The six RPDO4 commands for each control cycle are requested at the cycle boundary. After all six complete, SYNC is requested. After SYNC completes, six TPDO4 responses become ready after the configured node response delay.

A default node response delay of 0 us means CAN-only feasibility. It does not claim that a physical actuator has zero processing latency.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The standalone benchmark does not require SIL Kit.

## Run 500 Hz

```bash
./build/can_benchmark \
  --bitrate 1000000 \
  --frequency 500 \
  --duration 10 \
  --nodes 6 \
  --node-response-us 0 \
  --stuffing exact \
  --output-dir results/500Hz
```

Supported arguments:

```text
--bitrate <bit/s>
--frequency <Hz>
--duration <seconds>
--nodes <1..6>
--node-response-us <us>
--stuffing exact|none
--output-dir <path>
--matrix
```

## Run 250 / 500 / 1000 Hz

```bash
./build/can_benchmark --duration 10 --output-dir results/matrix --matrix
```

or:

```bash
./scripts/run_matrix.sh
```

The comparison table is calculated by the simulator. PASS/FAIL values are not hard-coded.

## CSV output

A normal run writes:

```text
frame_trace.csv
cycle_metrics.csv
summary.csv
```

`frame_trace.csv` contains per-frame request time, CAN ID, type, node, DLC, raw/stuff/total bits, arbitration/start/end/release timestamps, queue delay, duration, and arbitration wait count.

`cycle_metrics.csv` contains cycle start/deadline, completion time, duration, slack, deadline result, frame count, bus busy time, utilization, and arbitration waits.

`summary.csv` contains one-row run statistics including deadline misses, mean/max/p95/p99 cycle timing, slack, bus utilization, arbitration delay, frame counts, and PASS/FAIL.

## PASS / FAIL meaning

PASS means every required RPDO4/SYNC/TPDO4 exchange completed before the configured cycle deadline in this ideal virtual Classical CAN timing model.

FAIL means one or more cycles missed the deadline.

The result does not prove that the physical M8010 system supports the rate.

## Automated tests

The test suite covers:

- CAN-ID arbitration: `0x080` beats `0x481` and `0x501`
- frame serialization and intermission
- bitrate dependency
- DLC dependency
- low vs heavy bit stuffing
- deterministic repeated timestamps
- benchmark deadline classification

## SIL Kit Network Simulator adapter

The original SIL Kit architecture remains in the repository.

`MySimulatedCanController` now forwards frame requests into `MySimulatedNetwork`, where shared arbitration, bus occupancy, bitrate-based timing, transmit completion, delivery, and intermission are modeled.

`NetSimDemo.cpp` uses a 1 us virtual-time step and no wall-clock sleep.

To build the optional SIL Kit adapter:

```bash
cmake -S . -B build-silkit \
  -DBUILD_SILKIT_NETSIM=ON \
  -DSilKit_DIR=/path/to/SilKit/lib/cmake/SilKit
cmake --build build-silkit -j
```

## Scope and limitations

This repository is intentionally limited to deterministic CAN timing and bus-load validation. It does not add motor dynamics, robot kinematics, welding logic, GUI, MATLAB, STM32 firmware, or unrelated CANopen features.

This is not a substitute for a physical CAN benchmark.

Final validation still requires:

- real MCU
- real CAN transceiver
- real bus wiring and termination
- real actuators or representative CAN test nodes
- logic/CAN analyzer

Physical testing is still needed for real firmware scheduling, actuator processing delay, oscillator/transceiver effects, wiring behavior, errors, and retransmissions.
