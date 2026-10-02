#include "CanBusSimulator.hpp"

#include <cmath>
#include <iostream>

using namespace m8010;

namespace {

Scenario make_case(std::uint64_t delay_us)
{
    Scenario s;
    s.name = "command_release_delay_test";
    s.mode = ControlMode::InterpolatedPositionRpdo4;
    s.wire_timing = WireTimingMode::ExactPayload;
    s.nodes = 6;
    s.buses = 1;
    s.control_hz = 500.0;
    s.bitrate = 1'000'000;
    s.cycles = 100;
    s.command_release_delay_ns = delay_us * 1000ull;
    s.motor_response_delay_ns = 250'000ull;
    return s;
}

} // namespace

int main()
{
    const Result base = BenchmarkSimulator{make_case(0)}.Run();
    const Result delayed = BenchmarkSimulator{make_case(250)}.Run();

    const double cmd_shift = delayed.max_command_latency_us - base.max_command_latency_us;
    const double fb_shift = delayed.max_feedback_latency_us - base.max_feedback_latency_us;

    if (std::abs(cmd_shift - 250.0) > 1.0) {
        std::cerr << "command release delay was not reflected in command timing: shift=" << cmd_shift << " us\n";
        return 1;
    }
    if (std::abs(fb_shift - 250.0) > 1.0) {
        std::cerr << "command release delay was not reflected in feedback timing: shift=" << fb_shift << " us\n";
        return 1;
    }
    return 0;
}
