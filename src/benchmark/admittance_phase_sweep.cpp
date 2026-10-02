#include "CanBusSimulator.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace m8010;

namespace {

struct Profile {
    const char* name;
    WireTimingMode wire_timing;
    bool stress;
};

Scenario make_scenario(std::uint64_t release_delay_us, const Profile& p)
{
    Scenario s;
    s.name = std::string{"test6_phase_"} + p.name;
    s.mode = ControlMode::InterpolatedPositionRpdo4;
    s.wire_timing = p.wire_timing;
    s.nodes = 6;
    s.buses = 1;
    s.control_hz = 500.0;
    s.bitrate = 1'000'000;
    s.cycles = 5000;
    s.command_release_delay_ns = release_delay_us * 1000ull;
    s.motor_response_delay_ns = 250'000ull;

    if (p.stress) {
        s.heartbeat_period_ms = 100;
        s.sdo_period_ms = 250;
        s.inject_one_emcy = true;
        s.frame_error_probability = 0.0005;
        s.random_seed = 0x4D8610u;
    }
    return s;
}

} // namespace

int main()
{
    const std::vector<std::uint64_t> delays_us{0, 100, 250, 400, 500, 600, 750, 1000};
    const std::vector<Profile> profiles{
        {"nominal", WireTimingMode::ExactPayload, false},
        {"wire_bound", WireTimingMode::ConservativeBound, false},
        {"production_stress", WireTimingMode::ExactPayload, true},
    };

    std::cout << "\nM8010 Test 6 — phase-accurate admittance command-release timing\n";
    std::cout << "6 nodes, 1 bus, 1 Mbit/s, RPDO4+SYNC, 500 Hz, 250 us TPDO response assumption\n";
    std::cout << "The six RPDOs are actually released onto the simulated bus after the specified compute delay.\n";
    std::cout << "Heartbeat/SDO/TPDO traffic can therefore interact naturally with the shifted command phase.\n\n";

    std::cout << std::left
              << std::setw(12) << "compute(us)"
              << std::setw(20) << "profile"
              << std::setw(10) << "load%"
              << std::setw(14) << "maxCmd(us)"
              << std::setw(14) << "p99Cmd(us)"
              << std::setw(16) << "maxFeedback"
              << std::setw(16) << "p99Feedback"
              << std::setw(10) << "cmdMiss"
              << std::setw(10) << "fbMiss"
              << "screen\n";
    std::cout << std::string(128, '-') << '\n';

    for (auto d : delays_us) {
        for (const auto& p : profiles) {
            BenchmarkSimulator sim{make_scenario(d, p)};
            const Result r = sim.Run();

            std::cout << std::fixed << std::setprecision(1)
                      << std::setw(12) << d
                      << std::setw(20) << p.name
                      << std::setw(10) << r.offered_wire_load_pct
                      << std::setw(14) << r.max_command_latency_us
                      << std::setw(14) << r.p99_command_latency_us
                      << std::setw(16) << r.max_feedback_latency_us
                      << std::setw(16) << r.p99_feedback_latency_us
                      << std::setw(10) << r.command_deadline_misses
                      << std::setw(10) << r.feedback_deadline_misses
                      << r.screening << '\n';
        }
    }

    std::cout << "\nInterpretation: command latency is measured from the beginning of each 2 ms control period,\n"
                 "so it already includes the simulated F/T + filtering + admittance + kinematics compute delay.\n"
                 "Unlike Test 4A, the delay is not added afterward; it changes when CAN traffic actually becomes ready.\n";
    return 0;
}
