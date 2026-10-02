# Test 6 — Phase-Accurate Admittance Command-Release Timing

## Purpose

Replace the additive approximation used in Test 4A with an event-level timing test in which the six RPDO4 commands are actually released onto the simulated CAN bus **after** the assumed control-computation delay.

This matters because delaying the command release changes its phase relative to heartbeat, SDO, TPDO and error/retransmission traffic. Therefore the resulting latency is not always equal to a simple mathematical sum.

## Intended control chain

```text
cycle start / F/T sample
  -> filtering and compensation
  -> 6D admittance calculation
  -> Jacobian / IK / joint-reference generation
  -> command preparation
  -> six RPDO4 commands become ready
  -> CAN arbitration / serialization
  -> SYNC
  -> synchronized M8010 execution
  -> TPDO feedback
```

## Architecture under test

- 6 M8010 nodes
- one Classical CANopen bus
- 1 Mbit/s
- 500 Hz / 2 ms control period
- RPDO4 + SYNC synchronized position control
- 250 us assumed motor TPDO response delay
- 5000 cycles per case

Three communication profiles are used:

- **nominal:** exact payload-dependent Classical CAN frame timing
- **wire_bound:** conservative CAN stuffing upper bound
- **production_stress:** exact timing plus 100 ms heartbeat, 250 ms SDO, one EMCY event and low random retransmission sensitivity

## Command-release sweep

The aggregate control-computation delay was swept through:

```text
0, 100, 250, 400, 500, 600, 750, 1000 us
```

The delay represents the time from cycle start until all six cyclic RPDO commands are ready to enter CAN arbitration.

## Results

| Compute delay | Nominal max command | Wire-bound max command | Stress max command | Worst same-cycle feedback result |
|---:|---:|---:|---:|---|
| 0 us | 976 us | 1085 us | 1090 us | PASS |
| 100 us | 1076 us | 1185 us | 1190 us | PASS |
| 250 us | 1226 us | 1335 us | 1340 us | PASS |
| 400 us | 1376 us | 1485 us | 1490 us | PASS; reduced feedback margin |
| 500 us | 1476 us | 1585 us | 1590 us | PASS; marginal feedback margin |
| 600 us | 1576 us | 1685 us | 1690 us | feedback misses in wire-bound/stress |
| 750 us | 1726 us | 1835 us | 1840 us | same-cycle feedback fails in all profiles |
| 1000 us | 1976 us | 2085 us | 2090 us | command deadline fails in wire-bound/stress |

Detailed observed feedback values:

- **400 us compute:** 1693 us nominal, 1815 us wire-bound, 1806 us stress; zero deadline misses.
- **500 us compute:** 1793 us nominal, 1915 us wire-bound, 1906 us stress; zero deadline misses, but only about 85 us conservative same-cycle feedback margin remains.
- **600 us compute:** command execution still meets the 2 ms deadline, but wire-bound feedback reaches 2015 us and misses all 5000 same-cycle feedback deadlines; stress reaches 2006 us and misses 4 cycles.
- **750 us compute:** commands still meet 2 ms in all profiles, but same-cycle feedback exceeds 2 ms in all profiles.
- **1000 us compute:** nominal command timing barely fits at 1976 us, while wire-bound and stress command timing exceed 2 ms.

All unit tests passed before the sweep, including the dedicated `command_release_delay` test.

## Interpretation

The phase-accurate result closely confirms the earlier additive Test 4A estimate, but is stronger because the delayed command release is modeled inside CAN arbitration.

For the preferred one-bus 1 Mbit/s architecture:

> **A control-computation budget of about 400 us is a reasonable preliminary design target.**

A 500 us computation delay still meets all modeled command and same-cycle feedback deadlines, but the conservative feedback margin is small. Therefore 500 us should be treated as an upper edge for software design, not a comfortable target.

The strict same-cycle TPDO deadline is intentionally conservative. A real sampled controller may use pipelined feedback from the previous cycle. Therefore the 600–750 us cases do not prove that admittance control is impossible; they show that the strict `sample -> compute -> command -> execute -> all TPDO feedback back within the same 2 ms window` assumption no longer holds.

The more fundamental hard requirement is that the six synchronized commands reach SYNC before the 2 ms command deadline with acceptably low latency/jitter. Under this model, that requirement remains satisfied up to 750 us compute delay and becomes unsafe around 1000 us in conservative/stress conditions.

## Procurement / architecture implication

This test supports the current preferred communication architecture provided that the production M8010 firmware confirms:

- 1 Mbit/s Classical CANopen operation,
- synchronized RPDO4 + SYNC behavior,
- the documented PDO mapping on the supplied firmware,
- acceptable TPDO scheduling/response behavior.

For the STM32H745 implementation, the initial engineering target should be:

```text
F/T acquisition + filtering + admittance + Jacobian/IK + command preparation <= ~400 us
```

with actual execution time measured on hardware later.

## Difference from Test 4A

Test 4A used:

```text
total latency = measured CAN latency + assumed compute delay
```

Test 6 shifts the RPDO release time inside the event simulator itself:

```text
RPDO ready time = cycle start + command_release_delay
```

This allows background CAN traffic and retransmissions to interact with the shifted control phase.

## Limitations

This remains a communication timing model, not a full robot dynamics or STM32 execution model.

It does not include:

- measured STM32H745 execution time
- measured F/T sensor acquisition latency
- actual M8010 firmware TPDO scheduling relative to SYNC
- real transceiver, cable and welding-EMI behavior
- admittance dynamics, stability or human-perceived hand-guiding quality

The exact production M8010 firmware/EDS still needs supplier confirmation for 1 Mbit/s, RPDO4/SYNC behavior and TPDO timing.

## Run

```bash
bash scripts/run_admittance_phase_sweep.sh
```

The script rebuilds the project, runs the unit tests, and executes the dedicated phase-accurate sweep.
