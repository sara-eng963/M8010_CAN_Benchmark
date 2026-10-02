#include "M8010Profile.hpp"

#include <cmath>

namespace m8010 {
namespace {
std::vector<std::uint8_t> pack_u16_le(std::uint16_t value)
{
    return {static_cast<std::uint8_t>(value & 0xFFu), static_cast<std::uint8_t>((value >> 8) & 0xFFu)};
}
}

std::string to_string(ControlMode mode)
{
    switch (mode) {
    case ControlMode::InterpolatedPositionRpdo4: return "interp_rpdo4_sync";
    case ControlMode::AsyncPositionRpdo1: return "async_position_rpdo1";
    case ControlMode::AsyncVelocityRpdo3: return "async_velocity_rpdo3";
    }
    return "unknown";
}

std::string to_string(WireTimingMode mode)
{
    return mode == WireTimingMode::ExactPayload ? "exact_stuffing" : "conservative_stuff_bound";
}

CanFrame make_rpdo(ControlMode mode, int node, int cycle, std::int32_t command_value)
{
    CanFrame f;
    f.node = node;
    f.cycle = cycle;
    if (mode == ControlMode::InterpolatedPositionRpdo4) {
        f.id = static_cast<std::uint16_t>(0x500 + node);
        f.data = pack_i32_le(command_value);
        f.label = "RPDO4_target_position";
    } else if (mode == ControlMode::AsyncPositionRpdo1) {
        f.id = static_cast<std::uint16_t>(0x200 + node);
        f.data = {0x0F, 0x00, 0x01};
        f.data = concat(std::move(f.data), pack_i32_le(command_value));
        f.label = "RPDO1_control_mode_position";
    } else {
        f.id = static_cast<std::uint16_t>(0x400 + node);
        f.data = {0x0F, 0x00, 0x03};
        f.data = concat(std::move(f.data), pack_i32_le(command_value));
        f.label = "RPDO3_control_mode_velocity";
    }
    return f;
}

CanFrame make_tpdo(ControlMode mode, int node, int cycle, std::int32_t feedback_value)
{
    CanFrame f;
    f.node = node;
    f.cycle = cycle;
    if (mode == ControlMode::InterpolatedPositionRpdo4) {
        f.id = static_cast<std::uint16_t>(0x480 + node);
        f.label = "TPDO4_actual_position_status";
    } else if (mode == ControlMode::AsyncPositionRpdo1) {
        f.id = static_cast<std::uint16_t>(0x180 + node);
        f.label = "TPDO1_actual_position_status";
    } else {
        f.id = static_cast<std::uint16_t>(0x380 + node);
        f.label = "TPDO3_actual_velocity_status";
    }
    f.data = concat(pack_i32_le(feedback_value), pack_u16_le(0x0437));
    return f;
}

CanFrame make_sync(int bus_index, int cycle)
{
    CanFrame f;
    f.id = 0x080;
    f.label = "SYNC_bus" + std::to_string(bus_index + 1);
    f.cycle = cycle;
    return f;
}

CanFrame make_heartbeat(int node)
{
    CanFrame f;
    f.id = static_cast<std::uint16_t>(0x700 + node);
    f.data = {0x05};
    f.label = "Heartbeat";
    f.node = node;
    return f;
}

CanFrame make_sdo_request(int node, int sequence)
{
    (void)sequence;
    CanFrame f;
    f.id = static_cast<std::uint16_t>(0x600 + node);
    f.data = {0x40, 0x64, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00};
    f.label = "SDO_read_actual_position";
    f.node = node;
    return f;
}

CanFrame make_sdo_response(int node, int sequence)
{
    CanFrame f;
    f.id = static_cast<std::uint16_t>(0x580 + node);
    f.data = {0x43, 0x64, 0x60, 0x00, static_cast<std::uint8_t>(sequence & 0xFF), 0x00, 0x00, 0x00};
    f.label = "SDO_response_actual_position";
    f.node = node;
    return f;
}

CanFrame make_emcy(int node, int code)
{
    CanFrame f;
    f.id = static_cast<std::uint16_t>(0x080 + node);
    f.data = {static_cast<std::uint8_t>(code & 0xFF), static_cast<std::uint8_t>((code >> 8) & 0xFF),
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    f.label = "EMCY_stress";
    f.node = node;
    return f;
}

std::int32_t trajectory_value(int node, int cycle, double frequency_hz)
{
    const double t = static_cast<double>(cycle) / frequency_hz;
    const double phase = 0.45 * static_cast<double>(node - 1);
    const double value = 60000.0 * std::sin(2.0 * 3.14159265358979323846 * 0.37 * t + phase)
                       + 7500.0 * std::sin(2.0 * 3.14159265358979323846 * 1.1 * t + 0.2 * node);
    return static_cast<std::int32_t>(std::llround(value));
}

} // namespace m8010
