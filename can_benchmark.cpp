#include "benchmark/BenchmarkRunner.hpp"

#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using canbench::BenchmarkConfig;
using canbench::BenchmarkResult;
using canbench::BenchmarkRunner;
using canbench::StuffingMode;

namespace
{
void PrintUsage(const char* exe)
{
    std::cout << "Usage: " << exe << " [options]\n\n"
              << "Options:\n"
              << "  --bitrate <bit/s>          Default: 1000000\n"
              << "  --frequency <Hz>           Default: 500\n"
              << "  --duration <seconds>       Default: 10\n"
              << "  --nodes <1..6>             Default: 6\n"
              << "  --node-response-us <us>    Default: 0\n"
              << "  --stuffing exact|none      Default: exact\n"
              << "  --output-dir <path>        Default: results\n"
              << "  --matrix                   Run 250, 500, and 1000 Hz\n"
              << "  --help\n";
}

std::string RequireValue(int& i, int argc, char** argv, const std::string& option)
{
    if (i + 1 >= argc)
        throw std::invalid_argument("Missing value for " + option);
    return argv[++i];
}

BenchmarkConfig ParseArgs(int argc, char** argv, bool& matrix)
{
    BenchmarkConfig config;
    matrix = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--bitrate") config.bitrate = static_cast<std::uint32_t>(std::stoul(RequireValue(i, argc, argv, arg)));
        else if (arg == "--frequency") config.frequencyHz = std::stod(RequireValue(i, argc, argv, arg));
        else if (arg == "--duration") config.durationSeconds = std::stod(RequireValue(i, argc, argv, arg));
        else if (arg == "--nodes") config.nodes = std::stoi(RequireValue(i, argc, argv, arg));
        else if (arg == "--node-response-us") config.nodeResponseDelayUs = std::stod(RequireValue(i, argc, argv, arg));
        else if (arg == "--output-dir") config.outputDir = RequireValue(i, argc, argv, arg);
        else if (arg == "--stuffing")
        {
            const auto value = RequireValue(i, argc, argv, arg);
            if (value == "exact") config.stuffingMode = StuffingMode::Exact;
            else if (value == "none") config.stuffingMode = StuffingMode::None;
            else throw std::invalid_argument("--stuffing must be exact or none");
        }
        else if (arg == "--matrix") matrix = true;
        else if (arg == "--help")
        {
            PrintUsage(argv[0]);
            std::exit(0);
        }
        else throw std::invalid_argument("Unknown argument: " + arg);
    }
    return config;
}

void PrintMatrix(const std::vector<BenchmarkResult>& results)
{
    std::cout << "\nRate      Cycle      Avg Load    Worst Cycle    Misses       Result\n";
    for (const auto& result : results)
    {
        std::cout << std::fixed << std::setprecision(0) << std::setw(4) << result.config.frequencyHz << " Hz   "
                  << std::setprecision(3) << std::setw(7) << static_cast<double>(result.periodNs) / 1e6 << " ms   "
                  << std::setw(8) << result.summary.averageBusUtilizationPercent << " %   "
                  << std::setw(9) << result.summary.maxCycleUs / 1000.0 << " ms   "
                  << std::setw(6) << result.summary.deadlineMisses << "       "
                  << (result.summary.pass ? "PASS" : "FAIL") << '\n';
    }
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        bool matrix = false;
        auto config = ParseArgs(argc, argv, matrix);

        if (!matrix)
        {
            const auto result = BenchmarkRunner::Run(config);
            BenchmarkRunner::WriteCsv(result);
            BenchmarkRunner::PrintHumanReadable(result);
            return result.summary.pass ? 0 : 2;
        }

        const auto baseOutput = config.outputDir;
        std::vector<BenchmarkResult> results;
        for (double frequency : {250.0, 500.0, 1000.0})
        {
            config.frequencyHz = frequency;
            config.outputDir = (std::filesystem::path(baseOutput) /
                                (std::to_string(static_cast<int>(frequency)) + "Hz")).string();
            auto result = BenchmarkRunner::Run(config);
            BenchmarkRunner::WriteCsv(result);
            results.push_back(std::move(result));
        }
        PrintMatrix(results);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "can_benchmark: " << e.what() << '\n';
        return 1;
    }
}
