// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
        mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
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
            if (c->SecondsSinceLastPing() > (20 * 60) ){
                beammp_debugf("client {} ({}) timing out: {}", c->GetID(), c->GetName(), c->SecondsSinceLastPing());
                TimedOutClients.push_back(c);
            } else if (c->IsSynced() && c->SecondsSinceLastPing() > (1 * 60)) {
                beammp_debugf("client {} ({}) timing out: {}", c->GetName(), c->GetID(), c->SecondsSinceLastPing());
                TimedOutClients.push_back(c);
            } 

            return true;
        });
        for (auto& ClientToKick : TimedOutClients) {
            ClientToKick->Disconnect("Timeout");
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
