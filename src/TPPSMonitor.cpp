#include "TPPSMonitor.h"
#include "Client.h"
#include "TNetwork.h"

TPPSMonitor::TPPSMonitor(TServer& Server)
    : mServer(Server) {
    Application::SetPPS("-");
    Application::RegisterShutdownHandler([&] {
        if (mThread.joinable()) {
            debug("shutting down PPSMonitor");
            mShutdown = true;
            mThread.join();
            debug("shut down PPSMonitor");
        }
    });
    Start();
}
void TPPSMonitor::operator()() {
    RegisterThread("PPSMonitor");
    while (!mNetwork) {
        // hard spi
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    info("PPSMonitor starting");
    std::vector<std::shared_ptr<TClient>> TimedOutClients;
    while (!mShutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        int C = 0, V = 0;
        if (mServer.ClientCount() == 0) {
            Application::SetPPS("-");
            continue;
        }
        mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
            if (!ClientPtr.expired()) {
                auto c = ClientPtr.lock();
                if (c->GetCarCount() > 0) {
                    C++;
                    V += c->GetCarCount();
                }
                // kick on "no ping"
                if (c->SecondsSinceLastPing() > 10 && c->IsSynced()) {
                    TimedOutClients.push_back(c);
                } else if (!c->IsSynced()) {
                    c->UpdatePingTime();
                }
            }
            return true;
        });
        for (auto& ClientToKick : TimedOutClients) {
            Network().ClientKick(*ClientToKick, "Timeout (no ping for >10 seconds)");
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
