#include "CanBusSimulator.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

using namespace m8010;

int main()
{
    const std::vector<int> burst_lengths{0, 1, 2, 3, 5, 10, 20, 50};

    std::cout << "\nM8010 Test 3B — deterministic CAN burst-error sensitivity\n";
    std::cout << "6 nodes, 1 bus, 1 Mbit/s, RPDO4+SYNC, 500 Hz, 250 us motor response delay\n";
    std::cout << "Background traffic: 100 ms heartbeat, 250 ms SDO, one EMCY event\n";
    std::cout << "The burst begins at the simulation midpoint and forces N consecutive CAN attempts to fail.\n\n";

    std::cout << std::left
              << std::setw(12) << "burstN"
              << std::setw(12) << "retries"
              << std::setw(12) << "load%"
              << std::setw(14) << "maxCmd(us)"
              << std::setw(16) << "maxFeedback"
              << std::setw(12) << "cmdMiss"
              << std::setw(12) << "fbMiss"
              << std::setw(10) << "maxQ"
              << "screen\n";
    std::cout << std::string(112, '-') << '\n';

    for (int burst : burst_lengths) {
        Scenario s;
        s.name = "test3b_burst_errors";
        s.mode = ControlMode::InterpolatedPositionRpdo4;
        s.wire_timing = WireTimingMode::ExactPayload;
        s.nodes = 6;
        s.buses = 1;
        s.control_hz = 500.0;
        s.bitrate = 1'000'000;
        s.cycles = 5000;
        s.motor_response_delay_ns = 250'000ull;
        s.heartbeat_period_ms = 100;
        s.sdo_period_ms = 250;
        s.inject_one_emcy = true;
        s.frame_error_probability = 0.0;
        s.burst_error_attempts = burst;

        BenchmarkSimulator sim{s};
        const Result r = sim.Run();

        std::cout << std::fixed << std::setprecision(1)
                  << std::setw(12) << burst
                  << std::setw(12) << r.retries
                  << std::setw(12) << r.offered_wire_load_pct
                  << std::setw(14) << r.max_command_latency_us
                  << std::setw(16) << r.max_feedback_latency_us
                  << std::setw(12) << r.command_deadline_misses
                  << std::setw(12) << r.feedback_deadline_misses
                  << std::setw(10) << r.max_queue_depth
                  << r.screening << '\n';
    }

    std::cout << "\nInterpretation note: a burst of N means N back-to-back failed transmission attempts,\n"
                 "each followed by normal CAN retransmission behavior. This is a stress abstraction for\n"
                 "short correlated disturbances such as EMI; it is not a physical welding-EMI model.\n";

    return 0;
}
