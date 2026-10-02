# Validation scope and assumptions

## Decision being tested

Can six M8010(B)E17B50L robot joints share Classical CANopen with enough cyclic bandwidth and determinism for a 6-DoF welding robot and F/T-based admittance control?

## Vendor-specific facts represented

The detailed M-series/YZ CANopen documentation describes:

- Classical CANopen with 11-bit CANopen COB-IDs
- node IDs 1..127
- SYNC COB-ID `0x080`
- RPDO1: `0x200 + node`, DLC 7, controlword `0x6040` + mode `0x6060` + target `0x607A`, asynchronous
- TPDO1: `0x180 + node`, DLC 6, actual position `0x6064` + statusword `0x6041`, asynchronous response to RPDO1
- RPDO3: `0x400 + node`, DLC 7, controlword + speed mode + target speed
- TPDO3: `0x380 + node`, DLC 6, actual speed `0x606C` + statusword
- RPDO4: `0x500 + node`, DLC 4, target position `0x607A`, synchronous type 1
- TPDO4: `0x480 + node`, DLC 6, actual position + statusword, asynchronous response to RPDO4
- interpolation example completed every 2 ms
- configurable baud-rate values corresponding to 125k, 250k, 500k, and 1M

The product page for M8010(B)E17B50L states CANopen communication and 2 kHz location sampling.

## Important unknowns

These values are not publicly specified precisely enough, so the simulator does not pretend to know them:

1. Exact internal firmware processing delay between an RPDO reception and TPDO readiness.
2. Exact CAN controller transmit-mailbox behavior inside the M8010 firmware under simultaneous traffic.
3. Exact oscillator tolerance, transceiver delay, cable propagation, and EMC noise in the final welding cell.
4. Exact error rate in the final harness and grounding arrangement.
5. Whether the exact firmware/EDS shipped in the bulk order is identical to the public M-series/YZ documentation.
6. Whether TPDO4 data sampled immediately after RPDO4 should be interpreted as pre-SYNC or post-SYNC plant feedback. The timing benchmark treats it only as documented bus traffic.

The `--response-delay-us` option exists specifically to sweep unknown item 1.

## CAN timing model

For each Classical CAN 2.0A frame the model builds the actual bits from SOF, 11-bit identifier, RTR/IDE/r0, DLC, payload, CRC-15/CAN, CRC delimiter, ACK slot and delimiter, EOF, and 3-bit intermission.

Bit stuffing is computed through the CRC sequence. A separate conservative mode uses an upper bound for stuffing to avoid basing procurement on favorable payload patterns.

Arbitration is modeled by the standard CAN rule: the numerically lower 11-bit identifier wins when multiple frames are pending at an idle bus.

## Two-bus synchronization

For a 3+3 split, the controller waits until all six RPDO4 targets have been delivered, then releases one SYNC frame on each bus at the same logical time.

The two SYNC frames can still finish at slightly different times because one bus may already be transmitting a TPDO or other frame. This is why the simulator reports cross-bus execution skew instead of assuming two buses are automatically simultaneous.

If strict sub-100-us multi-axis simultaneity is required, the final controller implementation must verify STM32 FDCAN transmit timing and may need to reconfigure or suppress feedback traffic around the SYNC boundary.

## Robot-feature relevance

- **Welding trajectory execution:** use the RPDO4 + SYNC results; synchronized joint updates matter more than raw throughput alone.
- **F/T admittance hand guiding:** if admittance outputs cyclic joint position targets, RPDO4 + SYNC is directly relevant. If it outputs velocity commands, RPDO3 has a 7-byte command / 6-byte feedback pattern comparable to the RPDO1 asynchronous load.
- **Autonomous calibration / smart fault handling:** these features add computation at the master but do not inherently require extra high-rate PDO bandwidth. Low-rate diagnostics are represented by the stress profile.
- **HMI / logging:** should not be placed as heavy traffic on the motor-control CAN bus. The stress profile intentionally adds some diagnostic traffic to test margin.
- **Safety:** this timing benchmark is not a functional-safety assessment and does not establish STO, SIL, PL, or safe-motion functions.

## Stress profile

The default stress profile intentionally uses non-vendor assumptions:

- heartbeat every 100 ms per node
- SDO diagnostic read every 250 ms
- one 8-byte EMCY event during the run
- frame-error probability 0.0005 with conservative full-frame-loss + error-frame + retransmission cost

This profile is for sensitivity only. It should not be described as the motor's measured or guaranteed behavior.

## Procurement gate

Before approving the bulk order, obtain the exact EDS/manual/firmware version from the supplier and confirm that it matches the mappings above. If the supplied firmware differs, update `M8010Profile.cpp` and rerun the matrix before giving control approval.
