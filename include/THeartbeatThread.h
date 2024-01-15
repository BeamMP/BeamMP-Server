#pragma once

#include "Common.h"
#include "IThreaded.h"

class THeartbeatThread : public IThreaded {
public:
    THeartbeatThread(TResourceManager& ResourceManager, TServer& Server);
    //~THeartbeatThread();
    void operator()() override;

private:
    std::string GenerateCall();
    std::string GetPlayers();

    TResourceManager& mResourceManager;
    TServer& mServer;
};
