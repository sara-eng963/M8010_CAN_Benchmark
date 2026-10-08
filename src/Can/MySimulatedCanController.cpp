#include "MySimulatedCanController.hpp"

#include <sstream>

using namespace SilKit;
using namespace SilKit::Experimental::NetworkSimulation;
using namespace SilKit::Experimental::NetworkSimulation::Can;

MySimulatedCanController::MySimulatedCanController(MySimulatedNetwork* mySimulatedNetwork,
                                                   ControllerDescriptor controllerDescriptor)
    : _mySimulatedNetwork{mySimulatedNetwork}
    , _controllerDescriptor{controllerDescriptor}
{
    _logger = _mySimulatedNetwork->GetLogger();
    std::stringstream logMsg;
    logMsg << "Registered SimulatedCanController #" << static_cast<std::uint64_t>(controllerDescriptor)
           << " on network '" << _mySimulatedNetwork->GetNetworkName() << "'";
    _logger->Info(logMsg.str());
}

void MySimulatedCanController::OnSetControllerMode(const CanControllerMode& controllerMode)
{
    _controllerMode = controllerMode.state;
}

void MySimulatedCanController::OnSetBaudrate(const CanConfigureBaudrate& configureBaudrate)
{
    _baudRate = configureBaudrate.baudRate == 0U ? 1000000U : configureBaudrate.baudRate;
}

void MySimulatedCanController::OnFrameRequest(const CanFrameRequest& frameRequest)
{
    _mySimulatedNetwork->EnqueueCanFrame(_controllerDescriptor, frameRequest, _baudRate);
}
