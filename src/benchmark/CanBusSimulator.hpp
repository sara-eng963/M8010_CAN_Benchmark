#pragma once

#include "CanFrameTiming.hpp"
#include "M8010Profile.hpp"

#include <cstdint>
#include <functional>
#include <queue>
#include <random>
#include <string>
#include <vector>

namespace m8010 {

struct Scenario {
    std::string name;
    ControlMode mode = ControlMode::InterpolatedPositionRpdo4;
    WireTimingMode wire_timing = WireTimingMode::ExactPayload;
    int nodes = 6;
    int buses = 1;
    double control_hz = 500.0;
    std::uint64_t bitrate = 1'000'000;
    int cycles = 5000;
    std::uint64_t motor_response_delay_ns = 0;
    int heartbeat_period_ms = 0;
    int sdo_period_ms = 0;
    bool inject_one_emcy = false;
    double frame_error_probability = 0.0;
    std::uint32_t random_seed = 0x4D8010u;
};

struct Result {
    Scenario scenario;
    double offered_wire_load_pct = 0.0;
    double actual_bus_utilization_pct = 0.0;
    double headroom_pct = 0.0;
    double max_command_latency_us = 0.0;
    double p99_command_latency_us = 0.0;
    double max_execution_skew_us = 0.0;
    double p99_execution_skew_us = 0.0;
    double max_feedback_latency_us = 0.0;
    double p99_feedback_latency_us = 0.0;
    std::uint64_t command_deadline_misses = 0;
    std::uint64_t feedback_deadline_misses = 0;
    std::uint64_t retries = 0;
    std::size_t max_queue_depth = 0;
    double makespan_ms = 0.0;
    std::string screening;
};

class BenchmarkSimulator {
public:
    explicit BenchmarkSimulator(Scenario scenario);
    Result Run();

private:
    enum class FrameKind { Rpdo, Tpdo, Sync, Heartbeat, SdoRequest, SdoResponse, Emcy };

    struct PendingFrame {
        CanFrame frame;
        FrameKind kind;
        int bus = 0;
        std::uint64_t ready_ns = 0;
        std::uint64_t sequence = 0;
        std::uint64_t first_ready_ns = 0;
    };

    struct Event {
        std::uint64_t time_ns = 0;
        int priority = 0;
        std::uint64_t sequence = 0;
        std::function<void()> callback;
    };

    struct EventCompare {
        bool operator()(const Event& a, const Event& b) const
        {
            if (a.time_ns != b.time_ns) return a.time_ns > b.time_ns;
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.sequence > b.sequence;
        }
    };

    struct BusState {
        bool busy = false;
        std::vector<PendingFrame> pending;
        std::uint64_t successful_bits = 0;
        std::uint64_t attempted_bits = 0;
        std::uint64_t busy_ns = 0;
        std::size_t max_queue = 0;
    };

    struct CycleState {
        std::uint64_t start_ns = 0;
        std::uint64_t deadline_ns = 0;
        int rpdo_completed = 0;
        int feedback_completed = 0;
        int sync_completed = 0;
        std::vector<std::uint64_t> execution_ns;
        std::vector<std::uint64_t> rpdo_complete_ns;
        std::vector<std::uint64_t> feedback_complete_ns;
        bool sync_released = false;
    };

    Scenario _s;
    std::uint64_t _now_ns = 0;
    std::uint64_t _seq = 0;
    std::priority_queue<Event, std::vector<Event>, EventCompare> _events;
    std::vector<BusState> _buses;
    std::vector<CycleState> _cycles;
    std::mt19937 _rng;
    std::uniform_real_distribution<double> _uniform{0.0, 1.0};
    std::uint64_t _retries = 0;
    std::uint64_t _horizon_ns = 0;

    void Schedule(std::uint64_t time_ns, int priority, std::function<void()> callback);
    void Enqueue(PendingFrame frame);
    void TryStartBus(int bus_index);
    void OnTransmitSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns);
    void OnRpdoSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns);
    void OnTpdoSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns);
    void OnSyncSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns);
    void MaybeReleaseSync(int cycle);
    void GenerateInitialEvents();
    std::size_t WireBits(const CanFrame& frame) const;
    std::uint64_t BitsToNs(std::size_t bits) const;
    int BusForNode(int node) const;
    static double Percentile(std::vector<double> values, double q);
    Result BuildResult() const;
};

} // namespace m8010
