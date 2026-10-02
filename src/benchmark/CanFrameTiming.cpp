#include "CanFrameTiming.hpp"

#include <stdexcept>

namespace m8010 {
namespace {

using Bits = std::vector<int>;

void append_bits(Bits& bits, std::uint32_t value, int width)
{
    for (int i = width - 1; i >= 0; --i) bits.push_back((value >> i) & 1u);
}

std::uint16_t crc15_can(const Bits& bits)
{
    std::uint16_t crc = 0;
    constexpr std::uint16_t poly = 0x4599;
    for (int bit : bits) {
        const bool feedback = (((crc >> 14) & 1u) ^ static_cast<unsigned>(bit)) != 0;
        crc = static_cast<std::uint16_t>((crc << 1) & 0x7FFFu);
        if (feedback) crc ^= poly;
    }
    return crc;
}

std::size_t count_stuff_bits(const Bits& bits)
{
    int last = -1;
    int run = 0;
    std::size_t stuffed = 0;
    for (const int bit : bits) {
        if (bit == last) ++run;
        else { last = bit; run = 1; }
        if (run == 5) {
            ++stuffed;
            last = 1 - bit;
            run = 1;
        }
    }
    return stuffed;
}

} // namespace

FrameTiming classical_can_timing(const CanFrame& frame)
{
    if (frame.id > 0x7FFu) throw std::invalid_argument("Classical CAN 2.0A id must be <= 0x7FF");
    if (frame.data.size() > 8) throw std::invalid_argument("Classical CAN payload must be <= 8 bytes");

    Bits crc_input;
    crc_input.reserve(1 + 12 + 6 + frame.data.size() * 8);
    crc_input.push_back(0);
    append_bits(crc_input, frame.id, 11);
    crc_input.push_back(0);
    crc_input.push_back(0);
    crc_input.push_back(0);
    append_bits(crc_input, static_cast<std::uint32_t>(frame.data.size()), 4);
    for (auto byte : frame.data) append_bits(crc_input, byte, 8);

    const auto crc = crc15_can(crc_input);
    Bits stuffed_region = crc_input;
    append_bits(stuffed_region, crc, 15);

    const std::size_t stuffed = count_stuff_bits(stuffed_region);
    const std::size_t base = 47u + frame.data.size() * 8u;
    return FrameTiming{base, stuffed, base + stuffed};
}

std::size_t conservative_worst_case_bits(std::size_t payload_bytes)
{
    if (payload_bytes > 8) throw std::invalid_argument("Classical CAN payload must be <= 8 bytes");
    const std::size_t stuffable = 34u + payload_bytes * 8u;
    const std::size_t max_stuff_upper = (stuffable - 1u) / 4u;
    return 47u + payload_bytes * 8u + max_stuff_upper;
}

std::vector<std::uint8_t> pack_i32_le(std::int32_t value)
{
    const auto u = static_cast<std::uint32_t>(value);
    return {static_cast<std::uint8_t>(u & 0xFFu),
            static_cast<std::uint8_t>((u >> 8) & 0xFFu),
            static_cast<std::uint8_t>((u >> 16) & 0xFFu),
            static_cast<std::uint8_t>((u >> 24) & 0xFFu)};
}

std::vector<std::uint8_t> concat(std::vector<std::uint8_t> a, const std::vector<std::uint8_t>& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

} // namespace m8010
