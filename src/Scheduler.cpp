#include "Scheduler.hpp"

#include <stdexcept>
#include <utility>

Scheduler::Scheduler() = default;

void Scheduler::ScheduleEvent(std::chrono::nanoseconds delta, std::function<void()> callback)
{
    if (delta.count() < 0)
        throw std::invalid_argument("Scheduler cannot schedule a negative delay");
    ScheduleEventAt(_now + delta, std::move(callback));
}

void Scheduler::ScheduleEventAt(std::chrono::nanoseconds timestamp, std::function<void()> callback)
{
    if (timestamp < _now)
        throw std::invalid_argument("Scheduler cannot schedule an event in the past");
    _events.push(Event{timestamp, _nextSequence++, std::move(callback)});
}

void Scheduler::OnSimulationStep(std::chrono::nanoseconds now)
{
    if (now < _now)
        throw std::invalid_argument("Simulation time cannot move backwards");

    _now = now;
    while (!_events.empty() && _events.top().timestamp <= now)
    {
        Event event = _events.top();
        _events.pop();
        if (event.callback)
            event.callback();
    }
}
