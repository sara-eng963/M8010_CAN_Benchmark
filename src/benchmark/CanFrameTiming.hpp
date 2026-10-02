#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace m8010 {

struct CanFrame {
    std::uint16_t id = 0;
    std::vector<std::uint8_t> data;
    std::string label;
    int node = 0;
    int cycle = -1;
};

struct FrameTiming {
    std::size_t base_bits = 0;
    std::size_t stuffed_bits = 0;
    std::size_t total_bits = 0;
};

FrameTiming classical_can_timing(const CanFrame& frame);
std::size_t conservative_worst_case_bits(std::size_t payload_bytes);
std::vector<std::uint8_t> pack_i32_le(std::int32_t value);
std::vector<std::uint8_t> concat(std::vector<std::uint8_t> a, const std::vector<std::uint8_t>& b);

} // namespace m8010
