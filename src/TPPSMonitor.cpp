#include "TPPSMonitor.h"
#include "Client.h"
#include "TNetwork.h"

TPPSMonitor::TPPSMonitor(TServer& Server)
    : mServer(Server) {
    Application::SetSubsystemStatus("PPSMonitor", Application::Status::Starting);
    Application::SetPPS("-");
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("PPSMonitor", Application::Status::ShuttingDown);
        if (mThread.joinable()) {
            beammp_debug("shutting down PPSMonitor");
            mThread.join();
            beammp_debug("shut down PPSMonitor");
        }
        Application::SetSubsystemStatus("PPSMonitor", Application::Status::Shutdown);
    });
    Start();
}
void TPPSMonitor::operator()() {
    RegisterThread("PPSMonitor");
    while (!mNetwork) {
        // hard(-ish) spin
        std::this_thread::yield();
    }
    beammp_debug("PPSMonitor starting");
    Application::SetSubsystemStatus("PPSMonitor", Application::Status::Good);
    std::vector<std::shared_ptr<TClient>> TimedOutClients;
    while (!Application::IsShuttingDown()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        int C = 0, V = 0;
        if (mServer.ClientCount() == 0) {
            Application::SetPPS("-");
            continue;
        }
        mServer.ForEachClient([&](const auto& Client) {
            if (Client->GetCarCount() > 0) {
                C++;
                V += Client->GetCarCount();
            }
            // kick on "no ping"
            if (Client->SecondsSinceLastPing() > (20 * 60)) {
                beammp_debugf("Client {} ({}) timing out: {}s since last contact", Client->GetName(), Client->GetID(), Client->SecondsSinceLastPing());
                TimedOutClients.push_back(Client);
            }
        });
        for (auto& ClientToKick : TimedOutClients) {
            Network().ClientKick(*ClientToKick, "Timeout (no ping for way too long)");
        }
        TimedOutClients.clear();
        if (C == 0 || mInternalPPS == 0) {
            Application::SetPPS("-");
        } else {
            int R = (mInternalPPS / C) / V;
            Application::SetPPS(std::to_string(R));
        }
        mInternalPPS = 0;
    }
}
