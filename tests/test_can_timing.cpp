#include "CanBusSimulator.hpp"
#include "CanFrameTiming.hpp"
#include "M8010Profile.hpp"

#include <cstdlib>
#include <iostream>

using namespace m8010;

namespace {
int failures = 0;
void check(bool condition, const char* text)
{
    if (!condition) {
        std::cerr << "FAIL: " << text << '\n';
        ++failures;
    }
}
}

int main()
{
    for (std::size_t n = 0; n <= 8; ++n) {
        CanFrame f;
        f.id = 0x123;
        f.data.assign(n, 0x55);
        const auto t = classical_can_timing(f);
        check(t.base_bits == 47 + 8 * n, "base Classical CAN bit count");
        check(t.total_bits >= t.base_bits, "stuffing cannot reduce length");
        check(t.total_bits <= conservative_worst_case_bits(n), "exact frame within conservative stuffing bound");
    }

    const auto rpdo4 = make_rpdo(ControlMode::InterpolatedPositionRpdo4, 1, 0, 50000);
    const auto tpdo4 = make_tpdo(ControlMode::InterpolatedPositionRpdo4, 1, 0, 3448);
    check(rpdo4.id == 0x501 && rpdo4.data.size() == 4, "M8010 RPDO4 mapping");
    check(tpdo4.id == 0x481 && tpdo4.data.size() == 6, "M8010 TPDO4 mapping");

    const auto rpdo1 = make_rpdo(ControlMode::AsyncPositionRpdo1, 1, 0, 50000);
    const auto tpdo1 = make_tpdo(ControlMode::AsyncPositionRpdo1, 1, 0, 3448);
    check(rpdo1.id == 0x201 && rpdo1.data.size() == 7, "M8010 RPDO1 mapping");
    check(tpdo1.id == 0x181 && tpdo1.data.size() == 6, "M8010 TPDO1 mapping");

    Scenario s500;
    s500.mode = ControlMode::InterpolatedPositionRpdo4;
    s500.buses = 1;
    s500.control_hz = 500;
    s500.cycles = 200;
    const auto r500 = BenchmarkSimulator(s500).Run();
    check(r500.command_deadline_misses == 0, "one-bus 500 Hz interpolation should meet nominal command deadline");

    Scenario s1000 = s500;
    s1000.control_hz = 1000;
    const auto r1000 = BenchmarkSimulator(s1000).Run();
    check(r1000.command_deadline_misses > 0, "one-bus 1 kHz interpolation should overload/miss deadlines");

    Scenario two_bus = s500;
    two_bus.buses = 2;
    two_bus.control_hz = 1000;
    const auto r2 = BenchmarkSimulator(two_bus).Run();
    check(r2.command_deadline_misses == 0, "two-bus 1 kHz interpolation nominal should meet command deadline");

    if (failures == 0) std::cout << "All benchmark tests passed\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
