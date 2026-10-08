#pragma once

#include <cstdint>

#include "silkit/experimental/netsim/all.hpp"
#include "MySimulatedNetwork.hpp"

class MySimulatedCanController : public SilKit::Experimental::NetworkSimulation::Can::ISimulatedCanController
{
public:
    MySimulatedCanController(MySimulatedNetwork* mySimulatedNetwork,
                             SilKit::Experimental::NetworkSimulation::ControllerDescriptor controllerDescriptor);

    void OnSetBaudrate(const SilKit::Experimental::NetworkSimulation::Can::CanConfigureBaudrate& msg) override;
    void OnFrameRequest(const SilKit::Experimental::NetworkSimulation::Can::CanFrameRequest& msg) override;
    void OnSetControllerMode(const SilKit::Experimental::NetworkSimulation::Can::CanControllerMode& msg) override;

private:
    MySimulatedNetwork* _mySimulatedNetwork;
    SilKit::Experimental::NetworkSimulation::ControllerDescriptor _controllerDescriptor;
    std::uint32_t _baudRate{1000000U};
    SilKit::Services::Can::CanControllerState _controllerMode{};
    SilKit::Services::Logging::ILogger* _logger;
};
