#pragma once

#include "Client.h"
#include "Common.h"
#include "Compat.h"
#include "IThreaded.h"
#include "TServer.h"

class TResourceManager;

class TTCPServer : public IThreaded {
public:
    explicit TTCPServer(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager);
    ~TTCPServer();

    void operator()() override;

    bool TCPSend(TClient& c, const std::string& Data);
    void SendLarge(TClient& c, std::string Data);
    void Respond(TClient& c, const std::string& MSG, bool Rel);
    std::shared_ptr<TClient> CreateClient(SOCKET TCPSock);
    std::string TCPRcv(TClient& c);
    void ClientKick(TClient& c, const std::string& R);

    void SetUDPServer(TUDPServer& UDPServer);

    TUDPServer& UDPServer() { return mUDPServer->get(); }

    void SyncClient(std::weak_ptr<TClient> c);
    void Identify(SOCKET TCPSock);
    void Authentication(SOCKET TCPSock);
    bool CheckBytes(TClient& c, int32_t BytesRcv);
    void SyncResources(TClient& c);

    void UpdatePlayer(TClient& Client);

private:
    std::optional<std::reference_wrapper<TUDPServer>> mUDPServer { std::nullopt };
    TServer& mServer;
    TPPSMonitor& mPPSMonitor;
    TResourceManager& mResourceManager;
    bool mShutdown { false };

    void HandleDownload(SOCKET TCPSock);
    void OnConnect(std::weak_ptr<TClient> c);
    void TCPClient(std::weak_ptr<TClient> c);
    int OpenID();
    void OnDisconnect(std::weak_ptr<TClient> ClientPtr, bool kicked);
    void Parse(TClient& c, const std::string& Packet);
    void SendFile(TClient& c, const std::string& Name);
    static bool TCPSendRaw(SOCKET C, char* Data, int32_t Size);
    static void SplitLoad(TClient& c, int64_t Sent, int64_t Size, bool D, const std::string& Name);
};