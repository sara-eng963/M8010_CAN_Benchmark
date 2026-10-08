#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

class Scheduler
{
public:
    Scheduler();

    void ScheduleEvent(std::chrono::nanoseconds delta, std::function<void()> callback);
    void ScheduleEventAt(std::chrono::nanoseconds timestamp, std::function<void()> callback);
    void OnSimulationStep(std::chrono::nanoseconds now);

    std::chrono::nanoseconds Now() const
    {
        return _now;
    }

private:
    struct Event
    {
        std::chrono::nanoseconds timestamp;
        std::uint64_t sequence{0};
        std::function<void()> callback;
    };

    struct CompareEvent
    {
        bool operator()(const Event& lhs, const Event& rhs) const
        {
            if (lhs.timestamp != rhs.timestamp)
                return lhs.timestamp > rhs.timestamp;
            return lhs.sequence > rhs.sequence;
        }
    };

    std::priority_queue<Event, std::vector<Event>, CompareEvent> _events;
    std::chrono::nanoseconds _now{0};
    std::uint64_t _nextSequence{0};
};
