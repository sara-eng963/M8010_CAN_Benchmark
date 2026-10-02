# Test 3B — Deterministic CAN Burst-Error Robustness

## Purpose

Evaluate how the intended M8010 communication architecture behaves when CAN errors are correlated in a short burst rather than scattered randomly.

This is relevant because short disturbances in an industrial welding environment can create several consecutive failed CAN transmission attempts. The test is a communication stress abstraction only; it is **not** a physical electromagnetic-interference model of the welding process.

## Configuration

- 6 ATAR/Avatar M8010 nodes
- 1 Classical CANopen bus
- 1 Mbit/s
- 500 Hz control rate / 2 ms cycle
- synchronized RPDO4 + SYNC control
- 250 us assumed motor TPDO response delay
- 100 ms heartbeat traffic
- 250 ms SDO traffic
- one EMCY event
- deterministic burst inserted near the middle of the run
- normal CAN retransmission behavior after each failed attempt

## Results

| Consecutive failed attempts | Max command latency | Max feedback latency | Command misses | Feedback misses | Result |
|---:|---:|---:|---:|---:|---|
| 0 | 1085 us | 1400 us | 0 | 0 | PASS |
| 1 | 1226 us | 1541 us | 0 | 0 | PASS |
| 2 | 1367 us | 1682 us | 0 | 0 | PASS |
| 3 | 1508 us | 1823 us | 0 | 0 | CAUTION |
| 5 | 1790 us | 2105 us | 0 | 1 | FAIL |
| 10 | 3320 us | 3758 us | 1 | 1 | FAIL |
| 20 | 5558 us | 5961 us | 3 | 3 | FAIL |
| 50 | 13499 us | 13898 us | 8 | 8 | FAIL |

## Interpretation

The one-bus 500 Hz architecture tolerates isolated errors and very short bursts well.

- 1–2 consecutive failed attempts remain comfortably inside the 2 ms command and feedback deadlines.
- 3 consecutive failures still meet the deadline, but feedback latency rises to about 1.82 ms, leaving little margin.
- 5 consecutive failed attempts push feedback beyond 2 ms and produce a missed feedback deadline.
- larger bursts affect multiple control cycles and create growing queue depth and multi-millisecond delay.

The failure threshold is therefore not primarily average bus utilization. The average load remains close to the baseline because the burst is short relative to the entire run. The issue is **local timing disruption inside one or more 2 ms control periods**.

## Relevance to welding and admittance control

For coordinated welding, a short burst can delay a synchronized target update. For future admittance control, the more serious effect is stale feedback: the controller may temporarily operate on older joint-state information.

The final implementation should therefore include fault-handling rules such as:

- reject or hold a new admittance update when joint feedback is stale beyond the allowed age,
- monitor heartbeat / PDO age,
- transition to a safe hold or controlled stop after repeated missed cycles,
- log CAN error counters and bus-off events,
- do not treat automatic CAN retransmission as sufficient system-level fault handling.

## Conclusion

> At 500 Hz on one 1 Mbit/s bus, the communication model tolerates isolated CAN errors and bursts of up to about 2–3 consecutive failed attempts without a cyclic deadline miss. A burst of 5 consecutive failed attempts is enough to violate the 2 ms feedback deadline in the tested configuration.

This does **not** mean that 5 physical welding-noise events will necessarily cause failure; the test counts failed CAN transmission attempts, not measured EMI events.
