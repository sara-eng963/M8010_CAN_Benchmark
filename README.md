# M8010 CAN Benchmark

Pre-procurement timing benchmark for **six ATAR/Avatar M8010(B)E17B50L integrated robot joints** on Classical CANopen.

The goal is narrow: answer whether the CAN communication can support the robot's real-time cyclic motion traffic with enough timing margin for coordinated welding and F/T-sensor-based hand guiding. This is **not** a motor torque/thermal/mechanical model.

## What is modeled

The benchmark uses the M-series CANopen mappings documented by Avatar/ATAR:

| Path | Host -> motor | Motor -> host | Behavior |
|---|---|---|---|
| Interpolated position | RPDO4 `0x500 + node`, 4 B target position `0x607A` | TPDO4 `0x480 + node`, 6 B actual position `0x6064` + status `0x6041` | RPDO4 is synchronous type 1; target is applied after CANopen SYNC `0x080` |
| Asynchronous position | RPDO1 `0x200 + node`, 7 B controlword + mode + target | TPDO1 `0x180 + node`, 6 B actual position + status | Driver updates immediately after RPDO1 |
| Asynchronous velocity | RPDO3 `0x400 + node`, 7 B controlword + mode + target speed | TPDO3 `0x380 + node`, 6 B actual speed + status | Same bus-size class as RPDO1/TPDO1 |

The M-series manual describes position interpolation as a **2 ms cycle (500 Hz)** and gives 1 Mbit/s as a configurable CAN rate. The M8010 product data also states CANopen communication, a 15-bit single-turn + 16-bit multi-turn absolute encoder, and a 2 kHz position sampling frequency.

The simulator implements:

- Classical CAN 2.0A, 11-bit identifiers, maximum 8-byte payload
- bit-level frame construction
- CRC-15/CAN generation
- data-dependent CAN bit stuffing
- CAN identifier arbitration
- a single shared serialized bus
- one-bus 6-axis and two-bus 3+3 architectures
- asynchronous TPDO replies after each RPDO, as documented
- SYNC barrier for the RPDO4 interpolation path
- retransmission sensitivity after injected frame errors
- optional heartbeat, SDO diagnostic, and EMCY traffic
- command deadline, feedback deadline, queue depth, bus load, latency, and joint execution skew

## Run it

From the repository root:

```bash
bash scripts/run_benchmark.sh
```

For a faster smoke run:

```bash
bash scripts/run_benchmark.sh --quick
```

Results are written to:

```text
results/m8010_can_benchmark.csv
```

Useful sensitivity runs:

```bash
# Vendor's intended high CAN rate
bash scripts/run_benchmark.sh --bitrate 1000000

# See what happens if only 500 kbit/s is available
bash scripts/run_benchmark.sh --bitrate 500000

# Add an assumed 100 us firmware processing delay before TPDO becomes ready
bash scripts/run_benchmark.sh --response-delay-us 100
```

## Default test matrix

For each mode, the default matrix checks:

- 1 bus / 6 motors
- 2 buses / 3+3 motors
- 250 Hz
- 500 Hz
- 750 Hz
- 1000 Hz

Three network profiles are run:

1. **nominal** — exact CRC and data-dependent stuffing, cyclic motor traffic only.
2. **wire_bound** — conservative payload-independent stuffing upper bound.
3. **production_stress** — exact stuffing plus 100 ms heartbeats, a low-rate SDO read every 250 ms, one EMCY event, and a deliberately harsh 0.05% frame-error probability. These are engineering stress assumptions, **not claims about ATAR defaults**.

## Expected decision pattern

The deterministic model currently gives approximately these nominal results at 1 Mbit/s:

| Architecture / mode | Rate | Wire load | Main result |
|---|---:|---:|---|
| 1 bus, RPDO4 + SYNC | 500 Hz | ~58% exact / ~66% conservative | no command deadline misses; about 1.1–1.2 ms command-to-SYNC latency |
| 1 bus, RPDO4 + SYNC | 750 Hz | ~87% exact / ~99% conservative | little safety margin; not a robust design point |
| 1 bus, RPDO4 + SYNC | 1000 Hz | >100% | impossible to sustain |
| 2 buses, RPDO4 + SYNC | 1000 Hz | ~61% exact / ~69% conservative per busiest bus | feasible in the bus model, but cross-bus SYNC skew must be checked |
| 1 bus, asynchronous RPDO1 | 500 Hz | ~64% exact / ~72% conservative | bandwidth fits, but joint update skew is around 1.1 ms because axes execute as frames arrive |

For this robot, **RPDO4 + SYNC is the more relevant path for coordinated welding and smooth hand-guiding**, because all six joints execute together. The asynchronous path is included to show why bandwidth alone is not enough: it can fit while still producing significant inter-axis timing skew.

The screening labels (`PASS`, `CAUTION`, `MARGINAL`, `FAIL`) are engineering heuristics, not CANopen or ATAR certification limits. Always inspect the raw metrics.

## What this does not prove

This benchmark does not prove that the exact production firmware you will receive supports the same PDO/SYNC behavior at the requested rate. Before a bulk purchase, get written confirmation from the supplier that the exact **M8010(B)E17B50L firmware/EDS** supports:

- 1 Mbit/s Classical CANopen
- RPDO4 target-position mapping and SYNC operation
- the documented 2 ms interpolation cycle for all six nodes
- TPDO behavior and any minimum inhibit/processing times

See [`docs/VALIDATION_SCOPE.md`](docs/VALIDATION_SCOPE.md) for assumptions and limits.

## Sources used to parameterize the model

- ATAR M-series product page: https://www.atarrobot.com/harmonic-robot/m-series/M-robot-joint.html
- ATAR M-series download page (EDS/CANopen package): https://www.atarrobot.com/download/m/
- RobotAnno download listing for M8010L/YZ-AIM CANopen documentation: https://www.robotanno.com/en/download/page1/
- Public mirror of the Avatar M-series CANopen manual used for detailed PDO tables: https://www.scribd.com/document/1056243130/Driver-manual-for-YZ-CANOPEN-motors

The detailed manual source is a public mirror, so the exact firmware supplied with the purchased joints remains the procurement authority.
