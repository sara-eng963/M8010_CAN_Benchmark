#include "BenchmarkRunner.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace canbench
{
namespace
{
constexpr double kNsPerUs = 1000.0;
constexpr double kNsPerSecond = 1000000000.0;

struct CycleState
{
    int rpdoCompleted{0};
    int tpdoCompleted{0};
    bool syncRequested{false};
    std::int64_t completionNs{-1};
};

double NsToUs(std::int64_t ns)
{
    return static_cast<double>(ns) / kNsPerUs;
}

double Percentile(std::vector<double> values, double percentile)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const double rank = (percentile / 100.0) * static_cast<double>(values.size() - 1U);
    const auto low = static_cast<std::size_t>(std::floor(rank));
    const auto high = static_cast<std::size_t>(std::ceil(rank));
    if (low == high)
        return values[low];
    const double fraction = rank - static_cast<double>(low);
    return values[low] + (values[high] - values[low]) * fraction;
}

std::int64_t IntersectionNs(std::int64_t aStart, std::int64_t aEnd,
                            std::int64_t bStart, std::int64_t bEnd)
{
    const auto start = std::max(aStart, bStart);
    const auto end = std::min(aEnd, bEnd);
    return std::max<std::int64_t>(0, end - start);
}

void ValidateConfig(const BenchmarkConfig& config)
{
    if (config.bitrate == 0U)
        throw std::invalid_argument("--bitrate must be greater than zero");
    if (!(config.frequencyHz > 0.0))
        throw std::invalid_argument("--frequency must be greater than zero");
    if (!(config.durationSeconds > 0.0))
        throw std::invalid_argument("--duration must be greater than zero");
    if (config.nodes < 1 || config.nodes > 6)
        throw std::invalid_argument("--nodes must be in the range 1..6 for the configured M8010 PDO IDs");
    if (config.nodeResponseDelayUs < 0.0)
        throw std::invalid_argument("--node-response-us cannot be negative");
}
} // namespace

BenchmarkResult BenchmarkRunner::Run(const BenchmarkConfig& config)
{
    ValidateConfig(config);

    BenchmarkResult result;
    result.config = config;
    result.periodNs = static_cast<std::int64_t>(std::llround(kNsPerSecond / config.frequencyHz));
    if (result.periodNs <= 0)
        throw std::invalid_argument("Requested frequency is too high for nanosecond simulation resolution");

    const auto requestedCycles = static_cast<std::uint64_t>(
        std::floor(config.durationSeconds * config.frequencyHz + 1e-12));
    if (requestedCycles == 0U)
        throw std::invalid_argument("Simulation duration is shorter than one requested control cycle");

    const auto nodeResponseNs = static_cast<std::int64_t>(std::llround(config.nodeResponseDelayUs * kNsPerUs));
    std::vector<CycleState> state(requestedCycles);
    DeterministicCanBus bus(config.bitrate, config.stuffingMode);

    bus.SetCompletionHandler([&](const TransmissionRecord& tx) {
        auto& cycle = state.at(static_cast<std::size_t>(tx.frame.cycleIndex));
        switch (tx.frame.type)
        {
        case FrameType::RPDO4:
            ++cycle.rpdoCompleted;
            if (cycle.rpdoCompleted == config.nodes && !cycle.syncRequested)
            {
                cycle.syncRequested = true;
                CanFrameRequestModel sync;
                sync.canId = 0x080U;
                sync.type = FrameType::SYNC;
                sync.node = 0;
                sync.cycleIndex = tx.frame.cycleIndex;
                bus.RequestFrame(std::move(sync), tx.transmissionEndNs);
            }
            break;

        case FrameType::SYNC:
            for (int node = 1; node <= config.nodes; ++node)
            {
                CanFrameRequestModel tpdo;
                tpdo.canId = static_cast<std::uint32_t>(0x480 + node);
                tpdo.data = MakeTpdoPayload(tx.frame.cycleIndex, node);
                tpdo.type = FrameType::TPDO4;
                tpdo.node = node;
                tpdo.cycleIndex = tx.frame.cycleIndex;
                bus.RequestFrame(std::move(tpdo), tx.transmissionEndNs + nodeResponseNs);
            }
            break;

        case FrameType::TPDO4:
            ++cycle.tpdoCompleted;
            if (cycle.tpdoCompleted == config.nodes)
                cycle.completionNs = tx.transmissionEndNs;
            break;

        case FrameType::Other:
            break;
        }
    });

    for (std::uint64_t cycle = 0; cycle < requestedCycles; ++cycle)
    {
        const auto cycleStartNs = static_cast<std::int64_t>(cycle) * result.periodNs;
        for (int node = 1; node <= config.nodes; ++node)
        {
            CanFrameRequestModel rpdo;
            rpdo.canId = static_cast<std::uint32_t>(0x500 + node);
            rpdo.data = MakeRpdoPayload(cycle, node);
            rpdo.type = FrameType::RPDO4;
            rpdo.node = node;
            rpdo.cycleIndex = cycle;
            bus.RequestFrame(std::move(rpdo), cycleStartNs);
        }
    }

    bus.RunUntilIdle();
    result.frames = bus.CompletedTransmissions();

    result.cycles.resize(requestedCycles);
    for (std::uint64_t cycle = 0; cycle < requestedCycles; ++cycle)
    {
        auto& metrics = result.cycles[cycle];
        metrics.cycleIndex = cycle;
        metrics.cycleStartNs = static_cast<std::int64_t>(cycle) * result.periodNs;
        metrics.cycleDeadlineNs = metrics.cycleStartNs + result.periodNs;
        metrics.completionNs = state[cycle].completionNs;
        metrics.durationNs = metrics.completionNs >= 0 ? metrics.completionNs - metrics.cycleStartNs : 0;
        metrics.deadlineSlackNs = metrics.completionNs >= 0 ? metrics.cycleDeadlineNs - metrics.completionNs
                                                            : std::numeric_limits<std::int64_t>::min();
        metrics.deadlineMet = metrics.completionNs >= 0 && metrics.completionNs <= metrics.cycleDeadlineNs;
    }

    for (const auto& tx : result.frames)
    {
        if (tx.frame.cycleIndex < requestedCycles)
        {
            auto& owner = result.cycles[tx.frame.cycleIndex];
            ++owner.frameCount;
            owner.arbitrationWaits += tx.arbitrationWaits;
        }

        if (tx.busReleaseNs <= 0)
            continue;
        const auto firstWindow = std::max<std::int64_t>(0, tx.transmissionStartNs / result.periodNs);
        const auto lastWindow = std::min<std::int64_t>(
            static_cast<std::int64_t>(requestedCycles) - 1,
            (tx.busReleaseNs - 1) / result.periodNs);
        for (auto window = firstWindow; window <= lastWindow; ++window)
        {
            auto& cycle = result.cycles[static_cast<std::size_t>(window)];
            cycle.busBusyNs += IntersectionNs(tx.transmissionStartNs, tx.busReleaseNs,
                                              cycle.cycleStartNs, cycle.cycleDeadlineNs);
        }
    }

    for (auto& cycle : result.cycles)
    {
        cycle.busUtilizationPercent = 100.0 * static_cast<double>(cycle.busBusyNs) /
                                      static_cast<double>(result.periodNs);
    }

    auto& summary = result.summary;
    summary.requestedCycles = requestedCycles;
    summary.achievedCycles = static_cast<std::uint64_t>(
        std::count_if(state.begin(), state.end(), [](const CycleState& s) { return s.completionNs >= 0; }));
    summary.deadlineMisses = static_cast<std::uint64_t>(
        std::count_if(result.cycles.begin(), result.cycles.end(), [](const CycleMetrics& c) { return !c.deadlineMet; }));
    summary.deadlineMissPercent = 100.0 * static_cast<double>(summary.deadlineMisses) /
                                  static_cast<double>(summary.requestedCycles);

    std::vector<double> cycleUs;
    std::vector<double> slackUs;
    cycleUs.reserve(result.cycles.size());
    slackUs.reserve(result.cycles.size());
    for (const auto& cycle : result.cycles)
    {
        if (cycle.completionNs >= 0)
        {
            cycleUs.push_back(NsToUs(cycle.durationNs));
            slackUs.push_back(NsToUs(cycle.deadlineSlackNs));
        }
        summary.peakBusUtilizationPercent = std::max(summary.peakBusUtilizationPercent,
                                                     cycle.busUtilizationPercent);
    }

    if (!cycleUs.empty())
    {
        summary.averageCycleUs = std::accumulate(cycleUs.begin(), cycleUs.end(), 0.0) / cycleUs.size();
        summary.maxCycleUs = *std::max_element(cycleUs.begin(), cycleUs.end());
        summary.p95CycleUs = Percentile(cycleUs, 95.0);
        summary.p99CycleUs = Percentile(cycleUs, 99.0);
    }
    if (!slackUs.empty())
    {
        summary.minimumSlackUs = *std::min_element(slackUs.begin(), slackUs.end());
        summary.averageSlackUs = std::accumulate(slackUs.begin(), slackUs.end(), 0.0) / slackUs.size();
    }

    const auto scheduledEndNs = static_cast<std::int64_t>(requestedCycles) * result.periodNs;
    std::int64_t busyWithinScheduledWindowNs = 0;
    std::vector<double> arbitrationDelayUs;
    arbitrationDelayUs.reserve(result.frames.size());

    for (const auto& tx : result.frames)
    {
        busyWithinScheduledWindowNs += IntersectionNs(tx.transmissionStartNs, tx.busReleaseNs, 0, scheduledEndNs);
        arbitrationDelayUs.push_back(NsToUs(tx.queueDelayNs));
        ++summary.totalFrames;
        switch (tx.frame.type)
        {
        case FrameType::RPDO4: ++summary.rpdoFrames; break;
        case FrameType::SYNC: ++summary.syncFrames; break;
        case FrameType::TPDO4: ++summary.tpdoFrames; break;
        case FrameType::Other: break;
        }
    }

    summary.averageBusUtilizationPercent = 100.0 * static_cast<double>(busyWithinScheduledWindowNs) /
                                           static_cast<double>(scheduledEndNs);
    if (!arbitrationDelayUs.empty())
    {
        summary.averageArbitrationDelayUs =
            std::accumulate(arbitrationDelayUs.begin(), arbitrationDelayUs.end(), 0.0) / arbitrationDelayUs.size();
        summary.maximumArbitrationDelayUs = *std::max_element(arbitrationDelayUs.begin(), arbitrationDelayUs.end());
        summary.p95ArbitrationDelayUs = Percentile(arbitrationDelayUs, 95.0);
        summary.p99ArbitrationDelayUs = Percentile(arbitrationDelayUs, 99.0);
    }

    summary.pass = summary.achievedCycles == summary.requestedCycles && summary.deadlineMisses == 0U;
    return result;
}

void BenchmarkRunner::WriteCsv(const BenchmarkResult& result)
{
    namespace fs = std::filesystem;
    fs::create_directories(result.config.outputDir);

    {
        std::ofstream out(fs::path(result.config.outputDir) / "frame_trace.csv", std::ios::trunc);
        if (!out) throw std::runtime_error("Unable to create frame_trace.csv");
        out << "cycle_index,request_time_us,can_id,frame_type,node,dlc,raw_bits,stuff_bits,total_bits,"
               "arbitration_start_us,transmission_start_us,transmission_end_us,bus_release_us,queue_delay_us,"
               "transmission_duration_us,bus_occupancy_us,arbitration_waits\n";
        out << std::fixed << std::setprecision(3);
        for (const auto& tx : result.frames)
        {
            out << tx.frame.cycleIndex << ','
                << NsToUs(tx.requestTimeNs) << ','
                << "0x" << std::hex << std::uppercase << tx.frame.canId << std::dec << std::nouppercase << ','
                << ToString(tx.frame.type) << ','
                << tx.frame.node << ','
                << tx.frame.data.size() << ','
                << tx.timing.rawFrameBits << ','
                << tx.timing.stuffBits << ','
                << tx.timing.totalBusBits << ','
                << NsToUs(tx.arbitrationStartNs) << ','
                << NsToUs(tx.transmissionStartNs) << ','
                << NsToUs(tx.transmissionEndNs) << ','
                << NsToUs(tx.busReleaseNs) << ','
                << NsToUs(tx.queueDelayNs) << ','
                << NsToUs(tx.timing.frameDurationNs) << ','
                << NsToUs(tx.timing.busOccupancyNs) << ','
                << tx.arbitrationWaits << '\n';
        }
    }

    {
        std::ofstream out(fs::path(result.config.outputDir) / "cycle_metrics.csv", std::ios::trunc);
        if (!out) throw std::runtime_error("Unable to create cycle_metrics.csv");
        out << "cycle_index,cycle_start_us,cycle_deadline_us,last_required_frame_completion_us,"
               "cycle_completion_us,cycle_duration_us,deadline_slack_us,deadline_met,number_of_frames,"
               "bus_busy_us,bus_utilization_percent,arbitration_waits\n";
        out << std::fixed << std::setprecision(3);
        for (const auto& cycle : result.cycles)
        {
            out << cycle.cycleIndex << ','
                << NsToUs(cycle.cycleStartNs) << ','
                << NsToUs(cycle.cycleDeadlineNs) << ','
                << NsToUs(cycle.completionNs) << ','
                << NsToUs(cycle.completionNs) << ','
                << NsToUs(cycle.durationNs) << ','
                << NsToUs(cycle.deadlineSlackNs) << ','
                << (cycle.deadlineMet ? "true" : "false") << ','
                << cycle.frameCount << ','
                << NsToUs(cycle.busBusyNs) << ','
                << cycle.busUtilizationPercent << ','
                << cycle.arbitrationWaits << '\n';
        }
    }

    {
        std::ofstream out(fs::path(result.config.outputDir) / "summary.csv", std::ios::trunc);
        if (!out) throw std::runtime_error("Unable to create summary.csv");
        out << "can,bitrate,nodes,requested_frequency_hz,requested_cycle_us,duration_s,node_response_delay_us,"
               "stuffing,requested_cycles,achieved_cycles,deadline_misses,deadline_miss_percent,"
               "avg_cycle_us,max_cycle_us,p95_cycle_us,p99_cycle_us,min_slack_us,avg_slack_us,"
               "avg_bus_utilization_percent,peak_bus_utilization_percent,avg_arbitration_delay_us,"
               "max_arbitration_delay_us,p95_arbitration_delay_us,p99_arbitration_delay_us,total_frames,"
               "rpdo_frames,sync_frames,tpdo_frames,result\n";
        out << std::fixed << std::setprecision(3)
            << "Classical CAN 2.0A," << result.config.bitrate << ',' << result.config.nodes << ','
            << result.config.frequencyHz << ',' << NsToUs(result.periodNs) << ',' << result.config.durationSeconds << ','
            << result.config.nodeResponseDelayUs << ','
            << (result.config.stuffingMode == StuffingMode::Exact ? "exact" : "none") << ','
            << result.summary.requestedCycles << ',' << result.summary.achievedCycles << ','
            << result.summary.deadlineMisses << ',' << result.summary.deadlineMissPercent << ','
            << result.summary.averageCycleUs << ',' << result.summary.maxCycleUs << ','
            << result.summary.p95CycleUs << ',' << result.summary.p99CycleUs << ','
            << result.summary.minimumSlackUs << ',' << result.summary.averageSlackUs << ','
            << result.summary.averageBusUtilizationPercent << ',' << result.summary.peakBusUtilizationPercent << ','
            << result.summary.averageArbitrationDelayUs << ',' << result.summary.maximumArbitrationDelayUs << ','
            << result.summary.p95ArbitrationDelayUs << ',' << result.summary.p99ArbitrationDelayUs << ','
            << result.summary.totalFrames << ',' << result.summary.rpdoFrames << ',' << result.summary.syncFrames << ','
            << result.summary.tpdoFrames << ',' << (result.summary.pass ? "PASS" : "FAIL") << '\n';
    }
}

void BenchmarkRunner::PrintHumanReadable(const BenchmarkResult& result)
{
    std::cout << "========================================\n"
              << "CAN BENCHMARK RESULT\n"
              << "========================================\n"
              << "CAN:                 Classical CAN 2.0A\n"
              << "Bitrate:             " << std::fixed << std::setprecision(3)
              << (static_cast<double>(result.config.bitrate) / 1000000.0) << " Mbit/s\n"
              << "Nodes:               " << result.config.nodes << "\n"
              << "Requested cycle:     " << NsToUs(result.periodNs) / 1000.0 << " ms\n"
              << "Requested rate:      " << result.config.frequencyHz << " Hz\n"
              << "Simulation duration: " << result.config.durationSeconds << " s\n"
              << "Node response delay: " << result.config.nodeResponseDelayUs << " us\n\n"
              << "Average bus load:    " << result.summary.averageBusUtilizationPercent << " %\n"
              << "Peak bus load:       " << result.summary.peakBusUtilizationPercent << " %\n\n"
              << "Worst cycle:         " << result.summary.maxCycleUs / 1000.0 << " ms\n"
              << "P95 / P99 cycle:     " << result.summary.p95CycleUs / 1000.0 << " / "
              << result.summary.p99CycleUs / 1000.0 << " ms\n"
              << "Minimum slack:       " << result.summary.minimumSlackUs / 1000.0 << " ms\n\n"
              << "Deadline misses:     " << result.summary.deadlineMisses << " / "
              << result.summary.requestedCycles << "\n\n"
              << "RESULT: " << (result.summary.pass ? "PASS" : "FAIL") << "\n";

    if (result.summary.pass)
    {
        std::cout << result.config.frequencyHz
                  << " Hz traffic is feasible in this ideal error-free Classical CAN network timing model\n"
                  << "under the configured workload and assumptions.\n";
    }
    else
    {
        std::cout << result.summary.deadlineMisses << " cycles exceeded the configured "
                  << NsToUs(result.periodNs) / 1000.0 << " ms deadline.\n"
                  << "The requested traffic is infeasible in this simulated network timing model\n"
                  << "under the configured workload and assumptions.\n";
    }
    std::cout << "========================================\n";
}

std::vector<std::uint8_t> BenchmarkRunner::MakeRpdoPayload(std::uint64_t cycle, int node)
{
    const std::uint32_t target = static_cast<std::uint32_t>(
        (cycle * 100U + static_cast<std::uint64_t>(node) * 17U) & 0xFFFFFFFFU);
    return {
        static_cast<std::uint8_t>(target & 0xFFU),
        static_cast<std::uint8_t>((target >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((target >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((target >> 24U) & 0xFFU)
    };
}

std::vector<std::uint8_t> BenchmarkRunner::MakeTpdoPayload(std::uint64_t cycle, int node)
{
    const std::uint32_t position = static_cast<std::uint32_t>(
        (cycle * 97U + static_cast<std::uint64_t>(node) * 31U) & 0xFFFFFFFFU);
    const std::uint16_t status = static_cast<std::uint16_t>(0x1200U | static_cast<std::uint16_t>(node));
    return {
        static_cast<std::uint8_t>(position & 0xFFU),
        static_cast<std::uint8_t>((position >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((position >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((position >> 24U) & 0xFFU),
        static_cast<std::uint8_t>(status & 0xFFU),
        static_cast<std::uint8_t>((status >> 8U) & 0xFFU)
    };
}

} // namespace canbench
