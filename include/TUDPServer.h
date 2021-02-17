#pragma once

#include "Client.h"
#include "Common.h"
#include "Compat.h"
#include "IThreaded.h"
#include "TPPSMonitor.h"
#include "TServer.h"

class TUDPServer : public IThreaded {
public:
    explicit TUDPServer(TServer& Server, TPPSMonitor& PPSMonitor, TTCPServer& TCPServer);
    ~TUDPServer();

    void operator()() override;

    void UDPSend(TClient& Client, std::string Data) const;
    void SendToAll(TClient* c, const std::string& Data, bool Self, bool Rel);

private:
    TServer& mServer;
    TPPSMonitor& mPPSMonitor;
    TTCPServer& mTCPServer;
    SOCKET mUDPSock {};
    bool mShutdown { false };

    std::string UDPRcvFromClient(sockaddr_in& client) const;
};