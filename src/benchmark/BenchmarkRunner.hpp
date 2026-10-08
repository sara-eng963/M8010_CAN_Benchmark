#pragma once

#include "Can/DeterministicCanBus.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace canbench
{

struct BenchmarkConfig
{
    std::uint32_t bitrate{1000000U};
    double frequencyHz{500.0};
    double durationSeconds{10.0};
    int nodes{6};
    double nodeResponseDelayUs{0.0};
    StuffingMode stuffingMode{StuffingMode::Exact};
    std::string outputDir{"results"};
};

struct CycleMetrics
{
    std::uint64_t cycleIndex{0};
    std::int64_t cycleStartNs{0};
    std::int64_t cycleDeadlineNs{0};
    std::int64_t completionNs{0};
    std::int64_t durationNs{0};
    std::int64_t deadlineSlackNs{0};
    bool deadlineMet{false};
    std::uint64_t frameCount{0};
    std::int64_t busBusyNs{0};
    double busUtilizationPercent{0.0};
    std::uint64_t arbitrationWaits{0};
};

struct BenchmarkSummary
{
    std::uint64_t requestedCycles{0};
    std::uint64_t achievedCycles{0};
    std::uint64_t deadlineMisses{0};
    double deadlineMissPercent{0.0};
    double averageCycleUs{0.0};
    double maxCycleUs{0.0};
    double p95CycleUs{0.0};
    double p99CycleUs{0.0};
    double minimumSlackUs{0.0};
    double averageSlackUs{0.0};
    double averageBusUtilizationPercent{0.0};
    double peakBusUtilizationPercent{0.0};
    double averageArbitrationDelayUs{0.0};
    double maximumArbitrationDelayUs{0.0};
    double p95ArbitrationDelayUs{0.0};
    double p99ArbitrationDelayUs{0.0};
    std::uint64_t totalFrames{0};
    std::uint64_t rpdoFrames{0};
    std::uint64_t syncFrames{0};
    std::uint64_t tpdoFrames{0};
    bool pass{false};
};

struct BenchmarkResult
{
    BenchmarkConfig config;
    std::int64_t periodNs{0};
    std::vector<TransmissionRecord> frames;
    std::vector<CycleMetrics> cycles;
    BenchmarkSummary summary;
};

class BenchmarkRunner
{
public:
    static BenchmarkResult Run(const BenchmarkConfig& config);
    static void WriteCsv(const BenchmarkResult& result);
    static void PrintHumanReadable(const BenchmarkResult& result);

private:
    static std::vector<std::uint8_t> MakeRpdoPayload(std::uint64_t cycle, int node);
    static std::vector<std::uint8_t> MakeTpdoPayload(std::uint64_t cycle, int node);
};

} // namespace canbench
