#pragma once

#include "CanFrameTiming.hpp"

#include <cstdint>
#include <string>

namespace m8010 {

enum class ControlMode {
    InterpolatedPositionRpdo4,
    AsyncPositionRpdo1,
    AsyncVelocityRpdo3
};

enum class WireTimingMode {
    ExactPayload,
    ConservativeBound
};

std::string to_string(ControlMode mode);
std::string to_string(WireTimingMode mode);

CanFrame make_rpdo(ControlMode mode, int node, int cycle, std::int32_t command_value);
CanFrame make_tpdo(ControlMode mode, int node, int cycle, std::int32_t feedback_value);
CanFrame make_sync(int bus_index, int cycle);
CanFrame make_heartbeat(int node);
CanFrame make_sdo_request(int node, int sequence);
CanFrame make_sdo_response(int node, int sequence);
CanFrame make_emcy(int node, int code = 0x1000);
std::int32_t trajectory_value(int node, int cycle, double frequency_hz);

} // namespace m8010
