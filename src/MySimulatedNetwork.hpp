#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "silkit/SilKit.hpp"
#include "silkit/experimental/netsim/all.hpp"
#include "silkit/experimental/netsim/string_utils.hpp"

#include "Can/CanFrameTiming.hpp"
#include "Scheduler.hpp"

class MySimulatedNetwork : public SilKit::Experimental::NetworkSimulation::ISimulatedNetwork
{
public:
    MySimulatedNetwork(SilKit::IParticipant* participant, Scheduler* scheduler,
                       SilKit::Experimental::NetworkSimulation::SimulatedNetworkType networkType,
                       std::string networkName);

    auto ProvideSimulatedController(SilKit::Experimental::NetworkSimulation::ControllerDescriptor controllerDescriptor)
        -> SilKit::Experimental::NetworkSimulation::ISimulatedController* override;

    void SetEventProducer(
        std::unique_ptr<SilKit::Experimental::NetworkSimulation::IEventProducer> eventProducer) override;

    void SimulatedControllerRemoved(
        SilKit::Experimental::NetworkSimulation::ControllerDescriptor controllerDescriptor) override;

    void EnqueueCanFrame(
        SilKit::Experimental::NetworkSimulation::ControllerDescriptor sender,
        const SilKit::Experimental::NetworkSimulation::Can::CanFrameRequest& frameRequest,
        std::uint32_t bitrate);

    auto GetLogger() -> SilKit::Services::Logging::ILogger* { return _logger; }
    auto GetNetworkType() const -> SilKit::Experimental::NetworkSimulation::SimulatedNetworkType { return _networkType; }
    auto GetNetworkName() const -> const std::string& { return _networkName; }
    auto GetAllControllerDescriptors()
        -> SilKit::Util::Span<SilKit::Experimental::NetworkSimulation::ControllerDescriptor>
    {
        return SilKit::Util::ToSpan(_controllerDescriptors);
    }
    auto GetScheduler() -> Scheduler* { return _scheduler; }
    auto GetCanEventProducer() -> SilKit::Experimental::NetworkSimulation::Can::ICanEventProducer*
    {
        return static_cast<SilKit::Experimental::NetworkSimulation::Can::ICanEventProducer*>(_eventProducer.get());
    }

private:
    struct PendingCanFrame
    {
        std::uint64_t sequence{0};
        SilKit::Experimental::NetworkSimulation::ControllerDescriptor sender{};
        SilKit::Services::Can::CanFrame frame{};
        std::vector<std::uint8_t> payload;
        void* userContext{nullptr};
        std::uint32_t bitrate{1000000U};
        std::chrono::nanoseconds requestTime{0};
        std::uint32_t arbitrationWaits{0};
    };

    void ScheduleArbitration();
    void RunArbitration();
    void PublishFrameCompletion(PendingCanFrame pending, std::chrono::nanoseconds frameEnd);
    void ReleaseBus();

    SilKit::IParticipant* _participant;
    SilKit::Services::Logging::ILogger* _logger;
    Scheduler* _scheduler;
    SilKit::Experimental::NetworkSimulation::SimulatedNetworkType _networkType;
    std::string _networkName;

    std::unique_ptr<SilKit::Experimental::NetworkSimulation::IEventProducer> _eventProducer;
    std::vector<std::unique_ptr<SilKit::Experimental::NetworkSimulation::ISimulatedController>> _mySimulatedControllers;
    std::vector<SilKit::Experimental::NetworkSimulation::ControllerDescriptor> _controllerDescriptors;

    std::vector<PendingCanFrame> _pendingCanFrames;
    std::uint64_t _nextCanSequence{0};
    bool _canBusBusy{false};
    bool _arbitrationScheduled{false};
};
