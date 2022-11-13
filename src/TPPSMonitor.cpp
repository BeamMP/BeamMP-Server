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
        mServer.ForEachClientWeak([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
            std::shared_ptr<TClient> c;
            {
                ReadLock Lock(mServer.GetClientMutex());
                if (!ClientPtr.expired()) {
                    c = ClientPtr.lock();
                } else
                    return true;
            }
            if (c->GetCarCount() > 0) {
                C++;
                V += c->GetCarCount();
            }
            // kick on "no ping"
            if (c->SecondsSinceLastPing() > (20 * 60)) {
                beammp_debug("client " + std::string("(") + std::to_string(c->GetID()) + ")" + c->GetName() + " timing out: " + std::to_string(c->SecondsSinceLastPing()) + ", pps: " + Application::PPS());
                TimedOutClients.push_back(c);
            }

            return true;
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
