#pragma once

#include "Client.h"
#include "Common.h"
#include "Compat.h"
#include "IThreaded.h"
#include "TPPSMonitor.h"
#include "TServer.h"

class TUDPServer : public IThreaded {
public:
    explicit TUDPServer(TServer& Server, TPPSMonitor& PPSMonitor);

    void operator()() override;

    void UDPSend(TClient& Client, std::string Data) const;
    void SendToAll(TClient* c, const std::string& Data, bool Self, bool Rel);

private:
    void UDPParser(TClient& Client, std::string Packet);

    TServer& mServer;
    TPPSMonitor& mPPSMonitor;
    SOCKET mUDPSock;
    std::string UDPRcvFromClient(sockaddr_in& client) const;
};