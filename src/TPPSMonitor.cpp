#include "TPPSMonitor.h"
#include "Client.h"

TPPSMonitor::TPPSMonitor(TServer& Server)
    : mServer(Server) {
    Application::SetPPS("-");
    Application::RegisterShutdownHandler([&] { mShutdown = true; });
}
void TPPSMonitor::operator()() {
    while (!mShutdown) {
        int C = 0, V = 0;
        if (mServer.ClientCount() == 0) {
            Application::SetPPS("-");
            return;
        }
        mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
            if (!ClientPtr.expired()) {
                auto c = ClientPtr.lock();
                if (c->GetCarCount() > 0) {
                    C++;
                    V += c->GetCarCount();
                }
            }
            return true;
        });
        if (C == 0 || mInternalPPS == 0) {
            Application::SetPPS("-");
        } else {
            int R = (mInternalPPS / C) / V;
            Application::SetPPS(std::to_string(R));
        }
        mInternalPPS = 0;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
