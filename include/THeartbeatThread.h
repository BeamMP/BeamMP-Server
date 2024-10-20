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

#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "TResourceManager.h"
#include "TServer.h"

class THeartbeatThread : public IThreaded {
public:
    THeartbeatThread(TResourceManager& ResourceManager, TServer& Server);
    //~THeartbeatThread();
    void operator()() override;

    static inline std::string lastCall = "";
private:
    std::string GenerateCall();
    std::string GetPlayers();

    TResourceManager& mResourceManager;
    TServer& mServer;
};
