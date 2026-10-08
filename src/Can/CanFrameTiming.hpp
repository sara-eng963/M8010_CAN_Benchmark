#pragma once

#include <cstdint>
#include <vector>

namespace canbench
{

enum class StuffingMode
{
    Exact,
    None
};

struct CanFrameTiming
{
    std::uint32_t rawFrameBits{0};
    std::uint32_t stuffBits{0};
    std::uint32_t wireFrameBits{0};
    std::uint32_t intermissionBits{3};
    std::uint32_t totalBusBits{0};
    std::int64_t frameDurationNs{0};
    std::int64_t intermissionDurationNs{0};
    std::int64_t busOccupancyNs{0};
};

class CanFrameTimingCalculator
{
public:
    static CanFrameTiming AnalyzeStandardDataFrame(std::uint32_t canId,
                                                   const std::vector<std::uint8_t>& data,
                                                   std::uint32_t bitrate,
                                                   StuffingMode stuffingMode = StuffingMode::Exact);

private:
    static std::uint16_t ComputeCrc15(const std::vector<bool>& bits);
    static std::uint32_t CountStuffBits(const std::vector<bool>& bits);
    static std::int64_t BitsToNanoseconds(std::uint64_t bits, std::uint32_t bitrate);
};

} // namespace canbench
