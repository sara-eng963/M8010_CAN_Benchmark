#include "CanBusSimulator.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace m8010;

int main()
{
    const std::vector<double> probabilities{
        0.0,
        0.0001,
        0.0005,
        0.001,
        0.005,
        0.01,
        0.02,
        0.05,
        0.10,
    };

    const std::vector<std::uint32_t> seeds{
        0x4D8010u,
        0x4D8011u,
        0x4D8012u,
        0x4D8013u,
        0x4D8014u,
    };

    std::cout << "\nM8010 Test 3 — random CAN error/retransmission sensitivity\n";
    std::cout << "6 nodes, 1 bus, 1 Mbit/s, RPDO4+SYNC, 500 Hz, 250 us motor response delay\n";
    std::cout << "Background traffic: 100 ms heartbeat, 250 ms SDO, one EMCY event\n";
    std::cout << "Each error probability is run with 5 deterministic random seeds.\n\n";

    std::cout << std::left
              << std::setw(12) << "error%"
              << std::setw(12) << "avgRetry"
              << std::setw(12) << "maxLoad%"
              << std::setw(14) << "maxCmd(us)"
              << std::setw(16) << "maxFeedback"
              << std::setw(12) << "cmdMiss"
              << std::setw(12) << "fbMiss"
              << std::setw(10) << "maxQ"
              << "screen\n";
    std::cout << std::string(112, '-') << '\n';

    for (double p : probabilities) {
        double retry_sum = 0.0;
        double worst_load = 0.0;
        double worst_cmd = 0.0;
        double worst_fb = 0.0;
        std::uint64_t worst_cmd_miss = 0;
        std::uint64_t worst_fb_miss = 0;
        std::size_t worst_q = 0;
        std::string worst_screen = "PASS";

        for (auto seed : seeds) {
            Scenario s;
            s.name = "test3_random_errors";
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
            s.frame_error_probability = p;
            s.random_seed = seed;

            BenchmarkSimulator sim{s};
            const Result r = sim.Run();

            retry_sum += static_cast<double>(r.retries);
            worst_load = std::max(worst_load, r.offered_wire_load_pct);
            worst_cmd = std::max(worst_cmd, r.max_command_latency_us);
            worst_fb = std::max(worst_fb, r.max_feedback_latency_us);
            worst_cmd_miss = std::max(worst_cmd_miss, r.command_deadline_misses);
            worst_fb_miss = std::max(worst_fb_miss, r.feedback_deadline_misses);
            worst_q = std::max(worst_q, r.max_queue_depth);

            if (r.screening == "FAIL") worst_screen = "FAIL";
            else if (r.screening == "MARGINAL" && worst_screen != "FAIL") worst_screen = "MARGINAL";
            else if (r.screening == "CAUTION" && worst_screen == "PASS") worst_screen = "CAUTION";
        }

        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(12) << (100.0 * p)
                  << std::setprecision(1)
                  << std::setw(12) << (retry_sum / static_cast<double>(seeds.size()))
                  << std::setw(12) << worst_load
                  << std::setw(14) << worst_cmd
                  << std::setw(16) << worst_fb
                  << std::setw(12) << worst_cmd_miss
                  << std::setw(12) << worst_fb_miss
                  << std::setw(10) << worst_q
                  << worst_screen << '\n';
    }

    std::cout << "\nInterpretation note: the injected error probability is per CAN transmission attempt.\n"
                 "A failed attempt consumes modeled error-frame time and is automatically retransmitted.\n"
                 "The high percentages are deliberate stress cases, not expected field error rates.\n";

    return 0;
}
