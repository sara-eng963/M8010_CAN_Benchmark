#include "Can/CanFrameTiming.hpp"
#include "Can/DeterministicCanBus.hpp"
#include "benchmark/BenchmarkRunner.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace canbench;

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::vector<TransmissionRecord> ArbitrationScenario()
{
    DeterministicCanBus bus(1000000U, StuffingMode::Exact);
    bus.RequestFrame({0x501U, {1,2,3,4}, FrameType::RPDO4, 1, 0}, 0);
    bus.RequestFrame({0x080U, {}, FrameType::SYNC, 0, 0}, 0);
    bus.RequestFrame({0x481U, {1,2,3,4,5,6}, FrameType::TPDO4, 1, 0}, 0);
    bus.RunUntilIdle();
    return bus.CompletedTransmissions();
}

void TestArbitration()
{
    const auto tx = ArbitrationScenario();
    Require(tx.size() == 3U, "scenario should transmit three frames");
    Require(tx[0].frame.canId == 0x080U, "0x080 must win arbitration");
    Require(tx[1].frame.canId == 0x481U, "0x481 must transmit second");
    Require(tx[2].frame.canId == 0x501U, "0x501 must transmit last");
}

void TestSerialization()
{
    const auto tx = ArbitrationScenario();
    for (std::size_t i = 1; i < tx.size(); ++i)
        Require(tx[i].transmissionStartNs >= tx[i - 1].busReleaseNs, "frames must not overlap");
}

void TestBaudRateDependency()
{
    const std::vector<std::uint8_t> data{1,2,3,4};
    const auto fast = CanFrameTimingCalculator::AnalyzeStandardDataFrame(0x123U, data, 1000000U, StuffingMode::None);
    const auto slow = CanFrameTimingCalculator::AnalyzeStandardDataFrame(0x123U, data, 500000U, StuffingMode::None);
    Require(slow.busOccupancyNs == 2 * fast.busOccupancyNs, "500 kbit/s must take twice as long");
}

void TestDlcDependency()
{
    const auto dlc4 = CanFrameTimingCalculator::AnalyzeStandardDataFrame(
        0x123U, std::vector<std::uint8_t>(4, 0x55U), 1000000U, StuffingMode::Exact);
    const auto dlc6 = CanFrameTimingCalculator::AnalyzeStandardDataFrame(
        0x123U, std::vector<std::uint8_t>(6, 0x55U), 1000000U, StuffingMode::Exact);
    Require(dlc6.frameDurationNs > dlc4.frameDurationNs, "DLC 6 must take longer than DLC 4");
}

void TestStuffing()
{
    const auto low = CanFrameTimingCalculator::AnalyzeStandardDataFrame(
        0x123U, std::vector<std::uint8_t>(8, 0x55U), 1000000U, StuffingMode::Exact);
    const auto heavy = CanFrameTimingCalculator::AnalyzeStandardDataFrame(
        0x123U, std::vector<std::uint8_t>(8, 0x00U), 1000000U, StuffingMode::Exact);
    Require(heavy.stuffBits > low.stuffBits, "equal-bit runs must add more stuff bits");
}

void TestDeterminism()
{
    const auto a = ArbitrationScenario();
    const auto b = ArbitrationScenario();
    Require(a.size() == b.size(), "deterministic runs need equal frame count");
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        Require(a[i].frame.canId == b[i].frame.canId, "deterministic CAN order mismatch");
        Require(a[i].transmissionStartNs == b[i].transmissionStartNs, "deterministic start mismatch");
        Require(a[i].transmissionEndNs == b[i].transmissionEndNs, "deterministic end mismatch");
    }
}

void TestDeadlineBenchmark()
{
    BenchmarkConfig passConfig;
    passConfig.frequencyHz = 500.0;
    passConfig.durationSeconds = 0.020;
    const auto pass = BenchmarkRunner::Run(passConfig);
    Require(pass.summary.pass && pass.summary.deadlineMisses == 0U, "500 Hz should pass ideal model");

    BenchmarkConfig failConfig = passConfig;
    failConfig.frequencyHz = 1000.0;
    failConfig.durationSeconds = 0.010;
    const auto fail = BenchmarkRunner::Run(failConfig);
    Require(!fail.summary.pass && fail.summary.deadlineMisses > 0U, "1 kHz should miss deadlines");
}
} // namespace

int main()
{
    try
    {
        TestArbitration();
        TestSerialization();
        TestBaudRateDependency();
        TestDlcDependency();
        TestStuffing();
        TestDeterminism();
        TestDeadlineBenchmark();
        std::cout << "All CAN benchmark tests passed.\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "TEST FAILURE: " << e.what() << '\n';
        return 1;
    }
}
