#include "CanBusSimulator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace m8010 {

BenchmarkSimulator::BenchmarkSimulator(Scenario scenario)
    : _s(std::move(scenario)), _buses(static_cast<std::size_t>(_s.buses)), _rng(_s.random_seed)
{
    if (_s.nodes < 1 || _s.nodes > 127) throw std::invalid_argument("nodes must be 1..127");
    if (_s.buses < 1 || _s.buses > _s.nodes) throw std::invalid_argument("buses must be 1..nodes");
    if (_s.control_hz <= 0.0) throw std::invalid_argument("control_hz must be positive");
    if (_s.bitrate == 0) throw std::invalid_argument("bitrate must be positive");
    if (_s.cycles < 1) throw std::invalid_argument("cycles must be positive");
    if (_s.frame_error_probability < 0.0 || _s.frame_error_probability >= 1.0)
        throw std::invalid_argument("frame_error_probability must be in [0,1)");

    const auto period_ns = static_cast<std::uint64_t>(std::llround(1e9 / _s.control_hz));
    _horizon_ns = period_ns * static_cast<std::uint64_t>(_s.cycles);
    _cycles.resize(static_cast<std::size_t>(_s.cycles));
    for (int c = 0; c < _s.cycles; ++c) {
        auto& cs = _cycles[static_cast<std::size_t>(c)];
        cs.start_ns = static_cast<std::uint64_t>(c) * period_ns;
        cs.deadline_ns = cs.start_ns + period_ns;
        cs.execution_ns.assign(static_cast<std::size_t>(_s.nodes), 0);
        cs.rpdo_complete_ns.assign(static_cast<std::size_t>(_s.nodes), 0);
        cs.feedback_complete_ns.assign(static_cast<std::size_t>(_s.nodes), 0);
    }
}

void BenchmarkSimulator::Schedule(std::uint64_t time_ns, int priority, std::function<void()> callback)
{
    _events.push(Event{time_ns, priority, _seq++, std::move(callback)});
}

int BenchmarkSimulator::BusForNode(int node) const
{
    const int idx = node - 1;
    return std::min(_s.buses - 1, (idx * _s.buses) / _s.nodes);
}

std::size_t BenchmarkSimulator::WireBits(const CanFrame& frame) const
{
    if (_s.wire_timing == WireTimingMode::ConservativeBound)
        return conservative_worst_case_bits(frame.data.size());
    return classical_can_timing(frame).total_bits;
}

std::uint64_t BenchmarkSimulator::BitsToNs(std::size_t bits) const
{
    const long double ns = static_cast<long double>(bits) * 1.0e9L / static_cast<long double>(_s.bitrate);
    return static_cast<std::uint64_t>(std::ceil(ns));
}

void BenchmarkSimulator::Enqueue(PendingFrame frame)
{
    frame.sequence = _seq++;
    if (frame.first_ready_ns == 0 && frame.ready_ns != 0) frame.first_ready_ns = frame.ready_ns;
    Schedule(frame.ready_ns, 0, [this, frame = std::move(frame)]() mutable {
        auto& bus = _buses.at(static_cast<std::size_t>(frame.bus));
        bus.pending.push_back(std::move(frame));
        bus.max_queue = std::max(bus.max_queue, bus.pending.size());
        Schedule(_now_ns, 2, [this, b = static_cast<int>(&bus - _buses.data())]() { TryStartBus(b); });
    });
}

void BenchmarkSimulator::TryStartBus(int bus_index)
{
    auto& bus = _buses.at(static_cast<std::size_t>(bus_index));
    if (bus.busy || bus.pending.empty()) return;

    auto it = std::min_element(bus.pending.begin(), bus.pending.end(), [](const PendingFrame& a, const PendingFrame& b) {
        if (a.frame.id != b.frame.id) return a.frame.id < b.frame.id;
        return a.sequence < b.sequence;
    });
    PendingFrame pf = std::move(*it);
    bus.pending.erase(it);
    bus.busy = true;

    const auto frame_bits = WireBits(pf.frame);
    const bool fail = _uniform(_rng) < _s.frame_error_probability;
    const std::size_t consumed_bits = frame_bits + (fail ? 17u : 0u);
    const auto duration_ns = BitsToNs(consumed_bits);
    bus.attempted_bits += consumed_bits;
    bus.busy_ns += duration_ns;

    Schedule(_now_ns + duration_ns, 1, [this, bus_index, pf = std::move(pf), fail, frame_bits]() mutable {
        auto& b = _buses.at(static_cast<std::size_t>(bus_index));
        b.busy = false;
        if (fail) {
            ++_retries;
            pf.ready_ns = _now_ns;
            pf.sequence = _seq++;
            b.pending.push_back(std::move(pf));
            b.max_queue = std::max(b.max_queue, b.pending.size());
        } else {
            b.successful_bits += frame_bits;
            OnTransmitSuccess(pf, _now_ns);
        }
        Schedule(_now_ns, 2, [this, bus_index]() { TryStartBus(bus_index); });
    });
}

void BenchmarkSimulator::OnTransmitSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns)
{
    switch (pf.kind) {
    case FrameKind::Rpdo: OnRpdoSuccess(pf, tx_end_ns); break;
    case FrameKind::Tpdo: OnTpdoSuccess(pf, tx_end_ns); break;
    case FrameKind::Sync: OnSyncSuccess(pf, tx_end_ns); break;
    case FrameKind::SdoRequest: {
        PendingFrame reply;
        reply.frame = make_sdo_response(pf.frame.node, pf.frame.cycle);
        reply.kind = FrameKind::SdoResponse;
        reply.bus = pf.bus;
        reply.ready_ns = tx_end_ns + _s.motor_response_delay_ns;
        reply.first_ready_ns = reply.ready_ns;
        Enqueue(std::move(reply));
        break;
    }
    case FrameKind::Heartbeat:
    case FrameKind::SdoResponse:
    case FrameKind::Emcy:
        break;
    }
}

void BenchmarkSimulator::OnRpdoSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns)
{
    if (pf.frame.cycle < 0 || pf.frame.cycle >= _s.cycles) return;
    auto& cs = _cycles.at(static_cast<std::size_t>(pf.frame.cycle));
    const auto node_idx = static_cast<std::size_t>(pf.frame.node - 1);
    if (cs.rpdo_complete_ns[node_idx] == 0) {
        cs.rpdo_complete_ns[node_idx] = tx_end_ns;
        ++cs.rpdo_completed;
    }

    PendingFrame fb;
    fb.frame = make_tpdo(_s.mode, pf.frame.node, pf.frame.cycle,
                         trajectory_value(pf.frame.node, std::max(0, pf.frame.cycle - 1), _s.control_hz));
    fb.kind = FrameKind::Tpdo;
    fb.bus = pf.bus;
    fb.ready_ns = tx_end_ns + _s.motor_response_delay_ns;
    fb.first_ready_ns = fb.ready_ns;
    Enqueue(std::move(fb));

    if (_s.mode == ControlMode::InterpolatedPositionRpdo4) MaybeReleaseSync(pf.frame.cycle);
    else cs.execution_ns[node_idx] = tx_end_ns;
}

void BenchmarkSimulator::MaybeReleaseSync(int cycle)
{
    auto& cs = _cycles.at(static_cast<std::size_t>(cycle));
    if (cs.sync_released || cs.rpdo_completed != _s.nodes) return;
    cs.sync_released = true;
    for (int b = 0; b < _s.buses; ++b) {
        PendingFrame sync;
        sync.frame = make_sync(b, cycle);
        sync.kind = FrameKind::Sync;
        sync.bus = b;
        sync.ready_ns = _now_ns;
        sync.first_ready_ns = _now_ns;
        Enqueue(std::move(sync));
    }
}

void BenchmarkSimulator::OnTpdoSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns)
{
    if (pf.frame.cycle < 0 || pf.frame.cycle >= _s.cycles) return;
    auto& cs = _cycles.at(static_cast<std::size_t>(pf.frame.cycle));
    const auto node_idx = static_cast<std::size_t>(pf.frame.node - 1);
    if (cs.feedback_complete_ns[node_idx] == 0) {
        cs.feedback_complete_ns[node_idx] = tx_end_ns;
        ++cs.feedback_completed;
    }
}

void BenchmarkSimulator::OnSyncSuccess(const PendingFrame& pf, std::uint64_t tx_end_ns)
{
    if (pf.frame.cycle < 0 || pf.frame.cycle >= _s.cycles) return;
    auto& cs = _cycles.at(static_cast<std::size_t>(pf.frame.cycle));
    ++cs.sync_completed;
    for (int node = 1; node <= _s.nodes; ++node) {
        if (BusForNode(node) == pf.bus) cs.execution_ns[static_cast<std::size_t>(node - 1)] = tx_end_ns;
    }
}

void BenchmarkSimulator::GenerateInitialEvents()
{
    const auto period_ns = static_cast<std::uint64_t>(std::llround(1e9 / _s.control_hz));

    for (int c = 0; c < _s.cycles; ++c) {
        const auto t = static_cast<std::uint64_t>(c) * period_ns;
        for (int node = 1; node <= _s.nodes; ++node) {
            PendingFrame pf;
            pf.frame = make_rpdo(_s.mode, node, c, trajectory_value(node, c, _s.control_hz));
            pf.kind = FrameKind::Rpdo;
            pf.bus = BusForNode(node);
            pf.ready_ns = t;
            pf.first_ready_ns = t;
            Enqueue(std::move(pf));
        }
    }

    if (_s.heartbeat_period_ms > 0) {
        const auto hb_ns = static_cast<std::uint64_t>(_s.heartbeat_period_ms) * 1'000'000ull;
        for (int node = 1; node <= _s.nodes; ++node) {
            const auto phase = static_cast<std::uint64_t>(node - 1) * hb_ns / static_cast<std::uint64_t>(_s.nodes);
            for (std::uint64_t t = phase; t < _horizon_ns; t += hb_ns) {
                PendingFrame pf;
                pf.frame = make_heartbeat(node);
                pf.kind = FrameKind::Heartbeat;
                pf.bus = BusForNode(node);
                pf.ready_ns = t;
                pf.first_ready_ns = t;
                Enqueue(std::move(pf));
            }
        }
    }

    if (_s.sdo_period_ms > 0) {
        const auto sdo_ns = static_cast<std::uint64_t>(_s.sdo_period_ms) * 1'000'000ull;
        int seq = 0;
        for (std::uint64_t t = sdo_ns / 2; t < _horizon_ns; t += sdo_ns, ++seq) {
            const int node = (seq % _s.nodes) + 1;
            PendingFrame pf;
            pf.frame = make_sdo_request(node, seq);
            pf.frame.cycle = seq;
            pf.kind = FrameKind::SdoRequest;
            pf.bus = BusForNode(node);
            pf.ready_ns = t;
            pf.first_ready_ns = t;
            Enqueue(std::move(pf));
        }
    }

    if (_s.inject_one_emcy) {
        PendingFrame pf;
        pf.frame = make_emcy(1);
        pf.kind = FrameKind::Emcy;
        pf.bus = BusForNode(1);
        pf.ready_ns = _horizon_ns / 2;
        pf.first_ready_ns = pf.ready_ns;
        Enqueue(std::move(pf));
    }
}

double BenchmarkSimulator::Percentile(std::vector<double> values, double q)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double pos = q * static_cast<double>(values.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(pos));
    const auto hi = static_cast<std::size_t>(std::ceil(pos));
    if (lo == hi) return values[lo];
    const double alpha = pos - static_cast<double>(lo);
    return values[lo] * (1.0 - alpha) + values[hi] * alpha;
}

Result BenchmarkSimulator::BuildResult() const
{
    Result r;
    r.scenario = _s;
    r.retries = _retries;

    const double horizon_s = static_cast<double>(_horizon_ns) / 1e9;
    double max_offered = 0.0;
    double max_actual = 0.0;
    std::size_t max_q = 0;
    for (const auto& bus : _buses) {
        const double offered = 100.0 * static_cast<double>(bus.attempted_bits)
                             / (static_cast<double>(_s.bitrate) * horizon_s);
        max_offered = std::max(max_offered, offered);
        const double makespan_s = std::max(1e-12, static_cast<double>(_now_ns) / 1e9);
        const double actual = 100.0 * (static_cast<double>(bus.busy_ns) / 1e9) / makespan_s;
        max_actual = std::max(max_actual, actual);
        max_q = std::max(max_q, bus.max_queue);
    }
    r.offered_wire_load_pct = max_offered;
    r.actual_bus_utilization_pct = max_actual;
    r.headroom_pct = 100.0 - max_offered;
    r.max_queue_depth = max_q;
    r.makespan_ms = static_cast<double>(_now_ns) / 1e6;

    std::vector<double> command_lat;
    std::vector<double> feedback_lat;
    std::vector<double> skews;
    std::uint64_t cmd_miss = 0;
    std::uint64_t fb_miss = 0;

    for (const auto& cs : _cycles) {
        bool cmd_complete = true;
        bool fb_complete = true;
        std::uint64_t min_exec = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t max_exec = 0;
        std::uint64_t max_fb = 0;
        for (auto t : cs.execution_ns) {
            if (t == 0) { cmd_complete = false; break; }
            min_exec = std::min(min_exec, t);
            max_exec = std::max(max_exec, t);
        }
        for (auto t : cs.feedback_complete_ns) {
            if (t == 0) { fb_complete = false; break; }
            max_fb = std::max(max_fb, t);
        }

        if (!cmd_complete || max_exec > cs.deadline_ns) ++cmd_miss;
        if (!fb_complete || max_fb > cs.deadline_ns) ++fb_miss;

        if (cmd_complete) {
            command_lat.push_back(static_cast<double>(max_exec - cs.start_ns) / 1e3);
            skews.push_back(static_cast<double>(max_exec - min_exec) / 1e3);
        }
        if (fb_complete) feedback_lat.push_back(static_cast<double>(max_fb - cs.start_ns) / 1e3);
    }

    r.command_deadline_misses = cmd_miss;
    r.feedback_deadline_misses = fb_miss;
    r.max_command_latency_us = command_lat.empty() ? 0.0 : *std::max_element(command_lat.begin(), command_lat.end());
    r.p99_command_latency_us = Percentile(command_lat, 0.99);
    r.max_execution_skew_us = skews.empty() ? 0.0 : *std::max_element(skews.begin(), skews.end());
    r.p99_execution_skew_us = Percentile(skews, 0.99);
    r.max_feedback_latency_us = feedback_lat.empty() ? 0.0 : *std::max_element(feedback_lat.begin(), feedback_lat.end());
    r.p99_feedback_latency_us = Percentile(feedback_lat, 0.99);

    const double period_us = 1e6 / _s.control_hz;
    if (r.command_deadline_misses > 0 || r.offered_wire_load_pct >= 95.0) r.screening = "FAIL";
    else if (r.offered_wire_load_pct > 80.0 || r.max_command_latency_us > 0.90 * period_us) r.screening = "MARGINAL";
    else if (r.offered_wire_load_pct > 70.0 || r.max_command_latency_us > 0.75 * period_us) r.screening = "CAUTION";
    else r.screening = "PASS";
    return r;
}

Result BenchmarkSimulator::Run()
{
    GenerateInitialEvents();
    while (!_events.empty()) {
        Event ev = _events.top();
        _events.pop();
        _now_ns = ev.time_ns;
        if (ev.callback) ev.callback();
    }
    return BuildResult();
}

} // namespace m8010
