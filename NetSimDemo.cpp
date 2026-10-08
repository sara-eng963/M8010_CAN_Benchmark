// SPDX-FileCopyrightText: 2024 Vector Informatik GmbH
// SPDX-License-Identifier: MIT

#include <iostream>
#include <memory>
#include <string>

#include "silkit/SilKit.hpp"
#include "silkit/experimental/netsim/all.hpp"
#include "silkit/experimental/participant/ParticipantExtensions.hpp"
#include "silkit/services/all.hpp"
#include "silkit/services/orchestration/all.hpp"
#include "silkit/services/orchestration/string_utils.hpp"

#include "src/MySimulatedNetwork.hpp"

using namespace std::chrono_literals;
using namespace SilKit::Experimental::NetworkSimulation;

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <ParticipantConfiguration.yaml|json> <ParticipantName> [RegistryUri]\n";
        return -1;
    }

    const std::string participantConfigurationFilename(argv[1]);
    const std::string participantName(argv[2]);
    const std::string registryUri = argc >= 4 ? argv[3] : "silkit://localhost:8500";

    try
    {
        auto participantConfiguration =
            SilKit::Config::ParticipantConfigurationFromFile(participantConfigurationFilename);
        auto participant = SilKit::CreateParticipant(participantConfiguration, participantName, registryUri);

        auto* lifecycleService = participant->CreateLifecycleService(
            {SilKit::Services::Orchestration::OperationMode::Coordinated});
        auto* timeSyncService = lifecycleService->CreateTimeSyncService();

        auto scheduler = std::make_unique<Scheduler>();
        auto networkSimulator = SilKit::Experimental::Participant::CreateNetworkSimulator(participant.get());
        auto simulatedCanNetwork = std::make_unique<MySimulatedNetwork>(
            participant.get(), scheduler.get(), SimulatedNetworkType::CAN, "CAN1");
        networkSimulator->SimulateNetwork("CAN1", SimulatedNetworkType::CAN, std::move(simulatedCanNetwork));
        networkSimulator->Start();

        timeSyncService->SetSimulationStepHandler(
            [scheduler = scheduler.get()](std::chrono::nanoseconds now, std::chrono::nanoseconds) {
                scheduler->OnSimulationStep(now);
            },
            1us);

        const auto lifecycleFuture = lifecycleService->StartLifecycle();
        const auto finalState = lifecycleFuture.get();
        std::cout << "Simulation stopped. Final State: " << finalState << '\n';
    }
    catch (const SilKit::ConfigurationError& error)
    {
        std::cerr << "Invalid configuration: " << error.what() << '\n';
        return -2;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Something went wrong: " << error.what() << '\n';
        return -3;
    }

    return 0;
}
