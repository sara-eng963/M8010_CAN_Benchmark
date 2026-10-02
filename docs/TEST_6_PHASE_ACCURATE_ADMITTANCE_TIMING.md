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

The aggregate control-computation delay is swept through:

```text
0, 100, 250, 400, 500, 600, 750, 1000 us
```

The delay represents the time from cycle start until all six cyclic RPDO commands are ready to enter CAN arbitration.

## Metrics

For each delay and traffic profile the test reports:

- offered bus load
- maximum command-to-SYNC latency measured from cycle start
- p99 command latency
- maximum feedback age measured from cycle start
- p99 feedback age
- command deadline misses
- feedback deadline misses
- screening result

Because latency is measured from the start of the 2 ms control period, `maxCmd` already includes the assumed F/T + filtering + admittance + kinematics computation delay.

## Difference from Test 4A

Test 4A used:

```text
total latency = measured CAN latency + assumed compute delay
```

Test 6 instead shifts the RPDO release time inside the event simulator itself:

```text
RPDO ready time = cycle start + command_release_delay
```

This allows background CAN traffic and retransmissions to interact with the shifted control phase.

## Interpretation

The most important value for admittance is the **force-sample-to-SYNC actuation latency and its jitter**. Feedback age is also important, but the same-cycle 2 ms feedback deadline remains a deliberately conservative requirement because a real sampled controller may operate with pipelined state feedback.

A configuration that starts missing command deadlines is not acceptable for 500 Hz operation. A configuration that only misses same-cycle feedback deadlines requires deeper control-schedule analysis rather than being rejected automatically.

## Limitations

This remains a communication timing model, not a full robot dynamics or STM32 execution model.

It does not yet include:

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
