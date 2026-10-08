#include "MySimulatedNetwork.hpp"
#include "Can/MySimulatedCanController.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

using namespace SilKit::Experimental::NetworkSimulation;

MySimulatedNetwork::MySimulatedNetwork(SilKit::IParticipant* participant, Scheduler* scheduler,
                                       SimulatedNetworkType networkType, std::string networkName)
    : _participant{participant}
    , _logger{_participant->GetLogger()}
    , _scheduler{scheduler}
    , _networkType{networkType}
    , _networkName{std::move(networkName)}
{
    std::stringstream msg;
    msg << "Registered SimulatedNetwork '" << _networkName << "' of type '" << _networkType << "'";
    _logger->Info(msg.str());
}

void MySimulatedNetwork::SetEventProducer(std::unique_ptr<IEventProducer> eventProducer)
{
    _eventProducer = std::move(eventProducer);
}

auto MySimulatedNetwork::ProvideSimulatedController(ControllerDescriptor controllerDescriptor) -> ISimulatedController*
{
    _controllerDescriptors.push_back(controllerDescriptor);

    if (_networkType == SimulatedNetworkType::CAN)
    {
        _mySimulatedControllers.emplace_back(std::make_unique<MySimulatedCanController>(this, controllerDescriptor));
        return _mySimulatedControllers.back().get();
    }
    return nullptr;
}

void MySimulatedNetwork::SimulatedControllerRemoved(ControllerDescriptor controllerDescriptor)
{
    _controllerDescriptors.erase(
        std::remove(_controllerDescriptors.begin(), _controllerDescriptors.end(), controllerDescriptor),
        _controllerDescriptors.end());
}

void MySimulatedNetwork::EnqueueCanFrame(ControllerDescriptor sender,
                                         const Can::CanFrameRequest& frameRequest,
                                         std::uint32_t bitrate)
{
    PendingCanFrame pending;
    pending.sequence = _nextCanSequence++;
    pending.sender = sender;
    pending.frame = frameRequest.frame;
    pending.payload.assign(frameRequest.frame.dataField.begin(), frameRequest.frame.dataField.end());
    pending.userContext = frameRequest.userContext;
    pending.bitrate = bitrate == 0U ? 1000000U : bitrate;
    pending.requestTime = _scheduler->Now();
    _pendingCanFrames.push_back(std::move(pending));
    ScheduleArbitration();
}

void MySimulatedNetwork::ScheduleArbitration()
{
    if (_canBusBusy || _arbitrationScheduled || _pendingCanFrames.empty())
        return;

    _arbitrationScheduled = true;
    _scheduler->ScheduleEvent(std::chrono::nanoseconds{0}, [this] {
        _arbitrationScheduled = false;
        RunArbitration();
    });
}

void MySimulatedNetwork::RunArbitration()
{
    if (_canBusBusy || _pendingCanFrames.empty())
        return;

    const auto winner = std::min_element(
        _pendingCanFrames.begin(), _pendingCanFrames.end(),
        [](const PendingCanFrame& lhs, const PendingCanFrame& rhs) {
            if (lhs.frame.canId != rhs.frame.canId)
                return lhs.frame.canId < rhs.frame.canId;
            return lhs.sequence < rhs.sequence;
        });

    const auto winnerSequence = winner->sequence;
    for (auto& pending : _pendingCanFrames)
    {
        if (pending.sequence != winnerSequence)
            ++pending.arbitrationWaits;
    }

    PendingCanFrame selected = *winner;
    _pendingCanFrames.erase(winner);

    const auto timing = canbench::CanFrameTimingCalculator::AnalyzeStandardDataFrame(
        selected.frame.canId, selected.payload, selected.bitrate, canbench::StuffingMode::Exact);

    _canBusBusy = true;
    const auto frameDuration = std::chrono::nanoseconds{timing.frameDurationNs};
    const auto intermission = std::chrono::nanoseconds{timing.intermissionDurationNs};
    const auto frameEnd = _scheduler->Now() + frameDuration;

    _scheduler->ScheduleEvent(frameDuration, [this, selected, frameEnd, intermission]() mutable {
        PublishFrameCompletion(std::move(selected), frameEnd);
        _scheduler->ScheduleEvent(intermission, [this] { ReleaseBus(); });
    });
}

void MySimulatedNetwork::PublishFrameCompletion(PendingCanFrame pending, std::chrono::nanoseconds frameEnd)
{
    SilKit::Services::Can::CanFrameTransmitEvent ack;
    ack.canId = pending.frame.canId;
    ack.status = SilKit::Services::Can::CanTransmitStatus::Transmitted;
    ack.timestamp = frameEnd;
    ack.userContext = pending.userContext;

    std::array<ControllerDescriptor, 1> senderArray{pending.sender};
    GetCanEventProducer()->Produce(std::move(ack), SilKit::Util::MakeSpan(senderArray));

    SilKit::Services::Can::CanFrameEvent frameEvent;
    frameEvent.direction = SilKit::Services::TransmitDirection::RX;
    pending.frame.dataField = SilKit::Util::ToSpan(pending.payload);
    frameEvent.frame = pending.frame;
    frameEvent.userContext = pending.userContext;
    frameEvent.timestamp = frameEnd;
    GetCanEventProducer()->Produce(frameEvent, GetAllControllerDescriptors());
}

void MySimulatedNetwork::ReleaseBus()
{
    _canBusBusy = false;
    ScheduleArbitration();
}
