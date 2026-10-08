#pragma once

#include "CanFrameTiming.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <vector>

namespace canbench
{

enum class FrameType
{
    RPDO4,
    SYNC,
    TPDO4,
    Other
};

const char* ToString(FrameType type);

struct CanFrameRequestModel
{
    std::uint32_t canId{0};
    std::vector<std::uint8_t> data;
    FrameType type{FrameType::Other};
    int node{0};
    std::uint64_t cycleIndex{0};
};

struct TransmissionRecord
{
    std::uint64_t token{0};
    CanFrameRequestModel frame;
    std::int64_t requestTimeNs{0};
    std::int64_t arbitrationStartNs{0};
    std::int64_t transmissionStartNs{0};
    std::int64_t transmissionEndNs{0};
    std::int64_t busReleaseNs{0};
    std::int64_t queueDelayNs{0};
    std::uint32_t arbitrationWaits{0};
    CanFrameTiming timing;
};

class DeterministicCanBus
{
public:
    using CompletionHandler = std::function<void(const TransmissionRecord&)>;

    explicit DeterministicCanBus(std::uint32_t bitrate = 1000000U,
                                 StuffingMode stuffingMode = StuffingMode::Exact);

    std::uint64_t RequestFrame(CanFrameRequestModel frame, std::int64_t requestTimeNs);
    void AdvanceTo(std::int64_t targetTimeNs);
    void RunUntilIdle();

    void SetCompletionHandler(CompletionHandler handler);
    const std::vector<TransmissionRecord>& CompletedTransmissions() const;
    std::int64_t NowNs() const;
    bool HasWork() const;

private:
    struct PendingRequest
    {
        std::uint64_t token{0};
        CanFrameRequestModel frame;
        std::int64_t requestTimeNs{0};
        std::uint32_t arbitrationWaits{0};
    };

    struct FutureCompare
    {
        bool operator()(const PendingRequest& lhs, const PendingRequest& rhs) const;
    };

    struct ActiveTransmission
    {
        PendingRequest request;
        TransmissionRecord record;
        bool completionPublished{false};
    };

    void MoveReadyRequests(std::int64_t timeNs);
    void StartNextIfPossible();
    void PublishFrameCompletion();
    void ReleaseBus();
    std::int64_t NextInternalEventTime() const;

    std::uint32_t _bitrate;
    StuffingMode _stuffingMode;
    std::uint64_t _nextToken{1};
    std::int64_t _nowNs{0};

    std::priority_queue<PendingRequest, std::vector<PendingRequest>, FutureCompare> _future;
    std::vector<PendingRequest> _ready;
    std::optional<ActiveTransmission> _active;
    std::vector<TransmissionRecord> _completed;
    CompletionHandler _completionHandler;
};

} // namespace canbench
