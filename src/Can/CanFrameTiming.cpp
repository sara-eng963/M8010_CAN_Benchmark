#include "CanFrameTiming.hpp"

#include <stdexcept>

namespace canbench
{
namespace
{
constexpr std::uint16_t kCanCrc15Polynomial = 0x4599;

void AppendBits(std::vector<bool>& bits, std::uint32_t value, int bitCount)
{
    for (int bit = bitCount - 1; bit >= 0; --bit)
    {
        bits.push_back(((value >> bit) & 1U) != 0U);
    }
}
} // namespace

CanFrameTiming CanFrameTimingCalculator::AnalyzeStandardDataFrame(std::uint32_t canId,
                                                                  const std::vector<std::uint8_t>& data,
                                                                  std::uint32_t bitrate,
                                                                  StuffingMode stuffingMode)
{
    if (canId > 0x7FFU)
        throw std::invalid_argument("Classical CAN 2.0A requires an 11-bit CAN identifier");
    if (data.size() > 8U)
        throw std::invalid_argument("Classical CAN data frames support at most 8 data bytes");
    if (bitrate == 0U)
        throw std::invalid_argument("CAN bitrate must be greater than zero");

    std::vector<bool> crcInput;
    crcInput.reserve(1U + 12U + 6U + data.size() * 8U);

    crcInput.push_back(false);
    AppendBits(crcInput, canId, 11);
    crcInput.push_back(false);
    crcInput.push_back(false);
    crcInput.push_back(false);
    AppendBits(crcInput, static_cast<std::uint32_t>(data.size()), 4);

    for (std::uint8_t byte : data)
        AppendBits(crcInput, byte, 8);

    const std::uint16_t crc = ComputeCrc15(crcInput);
    std::vector<bool> stuffableBits = crcInput;
    AppendBits(stuffableBits, crc, 15);

    CanFrameTiming timing;
    timing.rawFrameBits = static_cast<std::uint32_t>(44U + data.size() * 8U);
    timing.stuffBits = stuffingMode == StuffingMode::Exact ? CountStuffBits(stuffableBits) : 0U;
    timing.wireFrameBits = timing.rawFrameBits + timing.stuffBits;
    timing.totalBusBits = timing.wireFrameBits + timing.intermissionBits;
    timing.frameDurationNs = BitsToNanoseconds(timing.wireFrameBits, bitrate);
    timing.intermissionDurationNs = BitsToNanoseconds(timing.intermissionBits, bitrate);
    timing.busOccupancyNs = timing.frameDurationNs + timing.intermissionDurationNs;
    return timing;
}

std::uint16_t CanFrameTimingCalculator::ComputeCrc15(const std::vector<bool>& bits)
{
    std::uint16_t remainder = 0U;
    for (bool bit : bits)
    {
        const bool feedback = (((remainder >> 14U) & 0x1U) != 0U) ^ bit;
        remainder = static_cast<std::uint16_t>((remainder << 1U) & 0x7FFFU);
        if (feedback)
            remainder ^= kCanCrc15Polynomial;
    }
    return remainder;
}

std::uint32_t CanFrameTimingCalculator::CountStuffBits(const std::vector<bool>& bits)
{
    if (bits.empty())
        return 0U;

    std::uint32_t stuffBits = 0U;
    bool previous = bits.front();
    int runLength = 1;

    for (std::size_t i = 1; i < bits.size(); ++i)
    {
        const bool bit = bits[i];
        if (bit == previous)
            ++runLength;
        else
        {
            previous = bit;
            runLength = 1;
        }

        if (runLength == 5)
        {
            ++stuffBits;
            previous = !previous;
            runLength = 1;
        }
    }
    return stuffBits;
}

std::int64_t CanFrameTimingCalculator::BitsToNanoseconds(std::uint64_t bits, std::uint32_t bitrate)
{
    constexpr std::uint64_t kNanosecondsPerSecond = 1000000000ULL;
    const std::uint64_t numerator = bits * kNanosecondsPerSecond;
    return static_cast<std::int64_t>((numerator + bitrate - 1U) / bitrate);
}

} // namespace canbench
