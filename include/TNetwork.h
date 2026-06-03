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

#include "BoostAliases.h"
#include "Compat.h"
#include "TConnectionLimiter.h"
#include "TResourceManager.h"
#include "TServer.h"
#include <boost/asio/ip/udp.hpp>

struct TConnection;

class TNetwork {
public:
    TNetwork(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager);

    [[nodiscard]] bool TCPSend(TClient& c, const std::vector<uint8_t>& Data, bool IsSync = false);
    [[nodiscard]] bool SendLarge(TClient& c, std::vector<uint8_t> Data, bool isSync = false);
    [[nodiscard]] bool Respond(TClient& c, const std::vector<uint8_t>& MSG, bool Rel, bool isSync = false);
    std::shared_ptr<TClient> CreateClient(boost::asio::ip::tcp::socket&& TCPSock);
    std::vector<uint8_t> TCPRcv(TClient& c, bool WithTimeout = false);
    void ClientKick(TClient& c, const std::string& R);
    void DisconnectClient(const std::weak_ptr<TClient>& c, const std::string& R);
    void DisconnectClient(TClient& c, const std::string& R);
    [[nodiscard]] bool SyncClient(const std::weak_ptr<TClient>& c);
    void Identify(TConnection&& client, TConnectionLimiter::TGuard&&);
    std::shared_ptr<TClient> Authentication(TConnection&& ClientConnection);
    void SyncResources(TClient& c);
    [[nodiscard]] bool UDPSend(TClient& Client, std::vector<uint8_t> Data);
    void SendToAll(TClient* c, const std::vector<uint8_t>& Data, bool Self, bool Rel);
    void UpdatePlayer(TClient& Client);
    boost::system::error_code ReadWithTimeout(boost::asio::ip::tcp::socket& Socket, void* Buf, size_t Len, std::chrono::steady_clock::duration Timeout);
    boost::system::error_code ReadWithTimeout(TConnection& Connection, void* Buf, size_t Len, std::chrono::steady_clock::duration Timeout);
    [[nodiscard]] TConnectionLimiter::TStats GetConnectionLimiterStats() { return mConnectionLimiter.GetStats(); }

    TResourceManager& ResourceManager() const { return mResourceManager; }

private:
    void UDPServerMain();
    void TCPServerMain();

    TServer& mServer;
    TPPSMonitor& mPPSMonitor;
    boost::asio::ip::udp::socket mUDPSock;
    TResourceManager& mResourceManager;
    std::thread mUDPThread;
    std::thread mTCPThread;
    std::mutex mOpenIDMutex;
    TConnectionLimiter mConnectionLimiter;

    std::vector<uint8_t> UDPRcvFromClient(boost::asio::ip::udp::endpoint& ClientEndpoint);
    void OnConnect(const std::weak_ptr<TClient>& c);
    void TCPClient(const std::weak_ptr<TClient>& c);
    void Looper(const std::weak_ptr<TClient>& c);
    int OpenID();
    void OnDisconnect(const std::weak_ptr<TClient>& ClientPtr);
    void Parse(TClient& c, const std::vector<uint8_t>& Packet);
    void SendFile(TClient& c, const std::string& Name);
    static bool TCPSendRaw(TClient& C, boost::asio::ip::tcp::socket& socket, const uint8_t* Data, size_t Size);
    void SendFileToClient(TClient& c, size_t Size, const std::string& Name);
    static const uint8_t* SendSplit(TClient& c, boost::asio::ip::tcp::socket& Socket, const uint8_t* DataPtr, size_t Size);
};

std::string HashPassword(const std::string& str);
std::vector<uint8_t> StringToVector(const std::string& Str);
