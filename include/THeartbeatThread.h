#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "Network.h"

class THeartbeatThread : public IThreaded {
public:
    THeartbeatThread(std::shared_ptr<Network> network);
    //~THeartbeatThread();
    void operator()() override;

private:
    std::string GenerateCall();
    std::string GetPlayers();

    std::shared_ptr<Network> m_network;
};
