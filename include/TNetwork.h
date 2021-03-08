#pragma once

#include "Compat.h"
#include "TResourceManager.h"
#include "TServer.h"

class TNetwork {
public:
    TNetwork(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager);

    bool TCPSend(TClient& c, const std::string& Data, bool IsSync = false);
    void SendLarge(TClient& c, std::string Data, bool isSync = false);
    void Respond(TClient& c, const std::string& MSG, bool Rel, bool isSync = false);
    std::shared_ptr<TClient> CreateClient(SOCKET TCPSock);
    std::string TCPRcv(TClient& c);
    void ClientKick(TClient& c, const std::string& R);
    void SyncClient(const std::weak_ptr<TClient>& c);
    void Identify(SOCKET TCPSock);
    void Authentication(SOCKET TCPSock);
    bool CheckBytes(TClient& c, int32_t BytesRcv);
    void SyncResources(TClient& c);
    void UDPSend(TClient& Client, std::string Data) const;
    void SendToAll(TClient* c, const std::string& Data, bool Self, bool Rel);
    void UpdatePlayer(TClient& Client);

private:
    void UDPServerMain();
    void TCPServerMain();

    TServer& mServer;
    TPPSMonitor& mPPSMonitor;
    SOCKET mUDPSock {};
    bool mShutdown { false };
    TResourceManager& mResourceManager;
    std::thread mUDPThread;
    std::thread mTCPThread;

    std::string UDPRcvFromClient(sockaddr_in& client) const;
    void HandleDownload(SOCKET TCPSock);
    void OnConnect(const std::weak_ptr<TClient>& c);
    void TCPClient(const std::weak_ptr<TClient>& c);
    int OpenID();
    void OnDisconnect(const std::weak_ptr<TClient>& ClientPtr, bool kicked);
    void Parse(TClient& c, const std::string& Packet);
    void SendFile(TClient& c, const std::string& Name);
    static bool TCPSendRaw(SOCKET C, char* Data, int32_t Size);
    static void SplitLoad(TClient& c, size_t Sent, size_t Size, bool D, const std::string& Name);
};
