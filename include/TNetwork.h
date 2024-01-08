#pragma once

#include "BoostAliases.h"
#include "Compat.h"
#include "TResourceManager.h"
#include "TServer.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

struct TConnection;

class TNetwork {
public:
    TNetwork(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager);

    [[nodiscard]] bool TCPSend(TClient& c, const std::vector<uint8_t>& Data, bool IsSync = false);
    [[nodiscard]] bool SendLarge(TClient& c, std::vector<uint8_t> Data, bool isSync = false);
    [[nodiscard]] bool Respond(TClient& c, const std::vector<uint8_t>& MSG, bool Rel, bool isSync = false);
    std::shared_ptr<TClient> CreateClient(ip::tcp::socket&& TCPSock);
    std::vector<uint8_t> TCPRcv(TClient& c);
    void ClientKick(TClient& c, const std::string& R);
    [[nodiscard]] bool SyncClient(const std::weak_ptr<TClient>& c);
    void Identify(TConnection&& client);
    std::shared_ptr<TClient> Authentication(TConnection&& ClientConnection);
    void SyncResources(TClient& c);
    [[nodiscard]] bool UDPSend(TClient& Client, std::vector<uint8_t> Data);
    void SendToAll(TClient* c, const std::vector<uint8_t>& Data, bool Self, bool Rel);
    void UpdatePlayer(TClient& Client);

private:
    void UDPServerMain();
    void TCPServerMain();

    TServer& mServer;
    TPPSMonitor& mPPSMonitor;
    ip::udp::socket mUDPSock;
    TResourceManager& mResourceManager;
    std::thread mUDPThread;
    std::thread mTCPThread;

    std::vector<uint8_t> UDPRcvFromClient(ip::udp::endpoint& ClientEndpoint);
    void HandleDownload(TConnection&& TCPSock);
    void OnConnect(const std::weak_ptr<TClient>& c);
    void TCPClient(const std::weak_ptr<TClient>& c);
    void Looper(const std::weak_ptr<TClient>& c);
    void OnDisconnect(const std::shared_ptr<TClient>& ClientPtr);
    void OnDisconnect(const std::weak_ptr<TClient>& ClientPtr);
    void OnDisconnect(TClient& Client);
    void Parse(TClient& c, const std::vector<uint8_t>& Packet);
    void SendFile(TClient& c, const std::string& Name);
    static bool TCPSendRaw(TClient& C, ip::tcp::socket& socket, const uint8_t* Data, size_t Size);
    static void SplitLoad(TClient& c, size_t Sent, size_t Size, bool D, const std::string& Name);
    static const uint8_t* SendSplit(TClient& c, ip::tcp::socket& Socket, const uint8_t* DataPtr, size_t Size);
};

std::string HashPassword(const std::string& str);
std::vector<uint8_t> StringToVector(const std::string& Str);
