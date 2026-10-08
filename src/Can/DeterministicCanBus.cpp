#include "DeterministicCanBus.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace canbench
{

const char* ToString(FrameType type)
{
    switch (type)
    {
    case FrameType::RPDO4: return "RPDO4";
    case FrameType::SYNC: return "SYNC";
    case FrameType::TPDO4: return "TPDO4";
    case FrameType::Other: return "OTHER";
    }
    return "OTHER";
}

DeterministicCanBus::DeterministicCanBus(std::uint32_t bitrate, StuffingMode stuffingMode)
    : _bitrate{bitrate}
    , _stuffingMode{stuffingMode}
{
    if (_bitrate == 0U)
        throw std::invalid_argument("CAN bitrate must be greater than zero");
}

std::uint64_t DeterministicCanBus::RequestFrame(CanFrameRequestModel frame, std::int64_t requestTimeNs)
{
    if (requestTimeNs < _nowNs)
        throw std::invalid_argument("Cannot enqueue a CAN frame in the simulated past");

    PendingRequest request;
    request.token = _nextToken++;
    request.frame = std::move(frame);
    request.requestTimeNs = requestTimeNs;
    const auto token = request.token;
    _future.push(std::move(request));
    return token;
}

void DeterministicCanBus::AdvanceTo(std::int64_t targetTimeNs)
{
    if (targetTimeNs < _nowNs)
        throw std::invalid_argument("Simulation time cannot move backwards");

    while (true)
    {
        MoveReadyRequests(_nowNs);
        StartNextIfPossible();

        if (_active && !_active->completionPublished && _active->record.transmissionEndNs == _nowNs)
        {
            PublishFrameCompletion();
            MoveReadyRequests(_nowNs);
        }
        if (_active && _active->record.busReleaseNs == _nowNs)
        {
            ReleaseBus();
            MoveReadyRequests(_nowNs);
            StartNextIfPossible();
        }

        const auto nextEvent = NextInternalEventTime();
        if (nextEvent == std::numeric_limits<std::int64_t>::max() || nextEvent > targetTimeNs)
        {
            _nowNs = targetTimeNs;
            MoveReadyRequests(_nowNs);
            StartNextIfPossible();
            if (_active && !_active->completionPublished && _active->record.transmissionEndNs == _nowNs)
            {
                PublishFrameCompletion();
                MoveReadyRequests(_nowNs);
            }
            if (_active && _active->record.busReleaseNs == _nowNs)
            {
                ReleaseBus();
                MoveReadyRequests(_nowNs);
                StartNextIfPossible();
            }
            return;
        }

        if (nextEvent == _nowNs)
            return;

        _nowNs = nextEvent;
    }
}

void DeterministicCanBus::RunUntilIdle()
{
    while (HasWork())
    {
        MoveReadyRequests(_nowNs);
        StartNextIfPossible();
        const auto next = NextInternalEventTime();
        if (next == std::numeric_limits<std::int64_t>::max())
            break;
        AdvanceTo(next);
    }
}

void DeterministicCanBus::SetCompletionHandler(CompletionHandler handler)
{
    _completionHandler = std::move(handler);
}

const std::vector<TransmissionRecord>& DeterministicCanBus::CompletedTransmissions() const
{
    return _completed;
}

std::int64_t DeterministicCanBus::NowNs() const
{
    return _nowNs;
}

bool DeterministicCanBus::HasWork() const
{
    return !_future.empty() || !_ready.empty() || _active.has_value();
}

bool DeterministicCanBus::FutureCompare::operator()(const PendingRequest& lhs, const PendingRequest& rhs) const
{
    if (lhs.requestTimeNs != rhs.requestTimeNs)
        return lhs.requestTimeNs > rhs.requestTimeNs;
    return lhs.token > rhs.token;
}

void DeterministicCanBus::MoveReadyRequests(std::int64_t timeNs)
{
    while (!_future.empty() && _future.top().requestTimeNs <= timeNs)
    {
        _ready.push_back(_future.top());
        _future.pop();
    }
}

void DeterministicCanBus::StartNextIfPossible()
{
    if (_active || _ready.empty())
        return;

    const auto winner = std::min_element(_ready.begin(), _ready.end(), [](const PendingRequest& lhs, const PendingRequest& rhs) {
        if (lhs.frame.canId != rhs.frame.canId)
            return lhs.frame.canId < rhs.frame.canId;
        return lhs.token < rhs.token;
    });

    for (auto& request : _ready)
    {
        if (request.token != winner->token)
            ++request.arbitrationWaits;
    }

    PendingRequest selected = *winner;
    _ready.erase(winner);

    const auto timing = CanFrameTimingCalculator::AnalyzeStandardDataFrame(
        selected.frame.canId, selected.frame.data, _bitrate, _stuffingMode);

    TransmissionRecord record;
    record.token = selected.token;
    record.frame = selected.frame;
    record.requestTimeNs = selected.requestTimeNs;
    record.arbitrationStartNs = _nowNs;
    record.transmissionStartNs = _nowNs;
    record.transmissionEndNs = _nowNs + timing.frameDurationNs;
    record.busReleaseNs = record.transmissionEndNs + timing.intermissionDurationNs;
    record.queueDelayNs = record.transmissionStartNs - record.requestTimeNs;
    record.arbitrationWaits = selected.arbitrationWaits;
    record.timing = timing;

    _active = ActiveTransmission{std::move(selected), std::move(record), false};
}

void DeterministicCanBus::PublishFrameCompletion()
{
    if (!_active || _active->completionPublished)
        return;

    _active->completionPublished = true;
    _completed.push_back(_active->record);
    if (_completionHandler)
        _completionHandler(_completed.back());
}

void DeterministicCanBus::ReleaseBus()
{
    if (_active && !_active->completionPublished)
        PublishFrameCompletion();
    _active.reset();
}

std::int64_t DeterministicCanBus::NextInternalEventTime() const
{
    std::int64_t next = std::numeric_limits<std::int64_t>::max();

    if (!_future.empty())
        next = std::min(next, _future.top().requestTimeNs);

    if (_active)
    {
        if (!_active->completionPublished)
            next = std::min(next, _active->record.transmissionEndNs);
        else
            next = std::min(next, _active->record.busReleaseNs);
    }

    return next;
}

} // namespace canbench
