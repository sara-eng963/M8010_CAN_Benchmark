#include "CanBusSimulator.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace m8010;

namespace {

struct Options {
    int cycles = 5000;
    std::string csv = "results/m8010_can_benchmark.csv";
    bool include_stress = true;
    bool include_conservative = true;
    std::uint64_t bitrate = 1'000'000;
    std::uint64_t response_delay_us = 0;
};

Options parse_args(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--cycles" && i + 1 < argc) o.cycles = std::stoi(argv[++i]);
        else if (a == "--csv" && i + 1 < argc) o.csv = argv[++i];
        else if (a == "--bitrate" && i + 1 < argc) o.bitrate = std::stoull(argv[++i]);
        else if (a == "--response-delay-us" && i + 1 < argc) o.response_delay_us = std::stoull(argv[++i]);
        else if (a == "--no-stress") o.include_stress = false;
        else if (a == "--no-conservative") o.include_conservative = false;
        else if (a == "--quick") o.cycles = 500;
        else if (a == "--help" || a == "-h") {
            std::cout << "M8010 CANopen timing benchmark\n"
                         "  --cycles N            cycles per scenario (default 5000)\n"
                         "  --csv PATH            CSV output path\n"
                         "  --bitrate BPS         CAN bitrate (default 1000000)\n"
                         "  --response-delay-us N motor firmware delay before TPDO becomes ready (default 0)\n"
                         "  --quick               500 cycles per scenario\n"
                         "  --no-stress           skip production-stress scenarios\n"
                         "  --no-conservative     skip stuffing upper-bound scenarios\n";
            std::exit(0);
        } else throw std::runtime_error("Unknown argument: " + a);
    }
    return o;
}

std::string profile_name(const Scenario& s)
{
    if (s.wire_timing == WireTimingMode::ConservativeBound) return "wire_bound";
    if (s.heartbeat_period_ms || s.sdo_period_ms || s.inject_one_emcy || s.frame_error_probability > 0.0)
        return "production_stress";
    return "nominal";
}

void write_csv_header(std::ostream& os)
{
    os << "scenario,mode,profile,buses,nodes,control_hz,bitrate,cycles,wire_timing,"
          "heartbeat_ms,sdo_ms,emcy,error_probability,response_delay_us,"
          "offered_wire_load_pct,actual_bus_utilization_pct,headroom_pct,"
          "max_command_latency_us,p99_command_latency_us,max_execution_skew_us,p99_execution_skew_us,"
          "max_feedback_latency_us,p99_feedback_latency_us,command_deadline_misses,feedback_deadline_misses,"
          "retries,max_queue_depth,makespan_ms,screening\n";
}

void write_csv_row(std::ostream& os, const Result& r)
{
    const auto& s = r.scenario;
    os << s.name << ',' << to_string(s.mode) << ',' << profile_name(s) << ',' << s.buses << ',' << s.nodes << ','
       << s.control_hz << ',' << s.bitrate << ',' << s.cycles << ',' << to_string(s.wire_timing) << ','
       << s.heartbeat_period_ms << ',' << s.sdo_period_ms << ',' << (s.inject_one_emcy ? 1 : 0) << ','
       << s.frame_error_probability << ',' << (static_cast<double>(s.motor_response_delay_ns) / 1000.0) << ','
       << r.offered_wire_load_pct << ',' << r.actual_bus_utilization_pct << ',' << r.headroom_pct << ','
       << r.max_command_latency_us << ',' << r.p99_command_latency_us << ',' << r.max_execution_skew_us << ','
       << r.p99_execution_skew_us << ',' << r.max_feedback_latency_us << ',' << r.p99_feedback_latency_us << ','
       << r.command_deadline_misses << ',' << r.feedback_deadline_misses << ',' << r.retries << ','
       << r.max_queue_depth << ',' << r.makespan_ms << ',' << r.screening << '\n';
}

Scenario base_scenario(ControlMode mode, int buses, double hz, int cycles, std::uint64_t bitrate, std::uint64_t response_delay_us)
{
    Scenario s;
    s.mode = mode;
    s.buses = buses;
    s.control_hz = hz;
    s.cycles = cycles;
    s.bitrate = bitrate;
    s.nodes = 6;
    s.motor_response_delay_ns = response_delay_us * 1000ull;
    s.name = to_string(mode) + "_" + std::to_string(buses) + "bus_" + std::to_string(static_cast<int>(hz)) + "Hz";
    return s;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const auto options = parse_args(argc, argv);
        const std::vector<double> frequencies{250.0, 500.0, 750.0, 1000.0};
        const std::vector<int> architectures{1, 2};
        const std::vector<ControlMode> modes{ControlMode::InterpolatedPositionRpdo4,
                                             ControlMode::AsyncPositionRpdo1};

        std::vector<Scenario> scenarios;
        for (auto mode : modes) {
            for (int buses : architectures) {
                for (double hz : frequencies) {
                    auto nominal = base_scenario(mode, buses, hz, options.cycles, options.bitrate, options.response_delay_us);
                    nominal.name += "_nominal";
                    scenarios.push_back(nominal);

                    if (options.include_conservative) {
                        auto bound = nominal;
                        bound.name = base_scenario(mode, buses, hz, options.cycles, options.bitrate, options.response_delay_us).name + "_wire_bound";
                        bound.wire_timing = WireTimingMode::ConservativeBound;
                        scenarios.push_back(bound);
                    }

                    if (options.include_stress) {
                        auto stress = nominal;
                        stress.name = base_scenario(mode, buses, hz, options.cycles, options.bitrate, options.response_delay_us).name + "_stress";
                        stress.heartbeat_period_ms = 100;
                        stress.sdo_period_ms = 250;
                        stress.inject_one_emcy = true;
                        stress.frame_error_probability = 0.0005;
                        stress.random_seed += static_cast<std::uint32_t>(hz) + static_cast<std::uint32_t>(31 * buses);
                        scenarios.push_back(stress);
                    }
                }
            }
        }

        const std::filesystem::path csv_path(options.csv);
        if (csv_path.has_parent_path()) std::filesystem::create_directories(csv_path.parent_path());
        std::ofstream csv(options.csv);
        if (!csv) throw std::runtime_error("Cannot open CSV output: " + options.csv);
        write_csv_header(csv);

        std::cout << "\nM8010 Classic CANopen communication benchmark\n"
                     "Primary model: 6 nodes, Classical CAN 2.0A, 11-bit IDs, vendor-documented PDO sizes/COB-IDs\n"
                     "Screening is an engineering margin heuristic; inspect raw metrics before procurement.\n\n";
        std::cout << std::left << std::setw(23) << "mode" << std::setw(8) << "buses" << std::setw(8) << "Hz"
                  << std::setw(19) << "profile" << std::setw(10) << "load%" << std::setw(13) << "maxCmd(us)"
                  << std::setw(12) << "skew(us)" << std::setw(10) << "misses" << "screen\n";
        std::cout << std::string(110, '-') << '\n';

        for (const auto& s : scenarios) {
            BenchmarkSimulator sim(s);
            const Result r = sim.Run();
            write_csv_row(csv, r);
            std::cout << std::left << std::setw(23) << to_string(s.mode) << std::setw(8) << s.buses
                      << std::setw(8) << static_cast<int>(s.control_hz) << std::setw(19) << profile_name(s)
                      << std::fixed << std::setprecision(1) << std::setw(10) << r.offered_wire_load_pct
                      << std::setw(13) << r.max_command_latency_us << std::setw(12) << r.max_execution_skew_us
                      << std::setw(10) << r.command_deadline_misses << r.screening << '\n';
        }

        std::cout << "\nCSV written to: " << options.csv << "\n";
        std::cout << "Important: TPDO response processing delay is unknown publicly and defaults to 0 us. "
                     "The stress profile is sensitivity testing, not a claim about ATAR's default network traffic.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
