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

#include "TNetwork.h"
#include "Client.h"
#include "Common.h"
#include "LuaAPI.h"
#include "TConnectionLimiter.h"
#include "THeartbeatThread.h"
#include "TLuaEngine.h"
#include "TScopedTimer.h"
#include "nlohmann/json.hpp"
#include <CustomAssert.h>
#include <Http.h>
#include <array>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/steady_timer.hpp>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <variant>
#include <zlib.h>

typedef boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO> rcv_timeout_option;

static constexpr uint8_t MAX_CONCURRENT_CONNECTIONS = 10;
static constexpr uint8_t MAX_GLOBAL_CONNECTIONS = 128;
static constexpr uint8_t READ_TIMEOUT_S = 10; // seconds

std::vector<uint8_t> StringToVector(const std::string& Str) {
    return std::vector<uint8_t>(Str.data(), Str.data() + Str.size());
}

static void CompressProperly(std::vector<uint8_t>& Data) {
    constexpr std::string_view ABG = "ABG:";
    auto CombinedData = std::vector<uint8_t>(ABG.begin(), ABG.end());
    auto CompData = Comp(Data);
    CombinedData.resize(ABG.size() + CompData.size());
    std::copy(CompData.begin(), CompData.end(), CombinedData.begin() + ABG.size());
    Data = CombinedData;
}

// for unit-tests, otherwise unused
static bool OpenLoopbackSocketPair(io_context& IoCtx, ip::tcp::socket& ClientSocket, ip::tcp::socket& ServerSocket, boost::system::error_code& Ec) {
    ip::tcp::acceptor Acceptor(IoCtx);
    Acceptor.open(ip::tcp::v4(), Ec);
    if (Ec) {
        return true;
    }
    Acceptor.bind(ip::tcp::endpoint(ip::address_v4::loopback(), 0), Ec);
    if (Ec) {
        return true;
    }
    Acceptor.listen(socket_base::max_listen_connections, Ec);
    if (Ec) {
        return true;
    }

    const auto Port = Acceptor.local_endpoint(Ec).port();
    if (Ec) {
        return true;
    }
    ClientSocket.connect(ip::tcp::endpoint(ip::address_v4::loopback(), Port), Ec);
    if (Ec) {
        return true;
    }
    Acceptor.accept(ServerSocket, Ec);
    return true;
}

TNetwork::TNetwork(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager)
    : mServer(Server)
    , mPPSMonitor(PPSMonitor)
    , mUDPSock(Server.IoCtx())
    , mResourceManager(ResourceManager)
    , mConnectionLimiter(MAX_CONCURRENT_CONNECTIONS, MAX_GLOBAL_CONNECTIONS) {
    Application::SetSubsystemStatus("TCPNetwork", Application::Status::Starting);
    Application::SetSubsystemStatus("UDPNetwork", Application::Status::Starting);
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("UDPNetwork", Application::Status::ShuttingDown);
        if (mUDPThread.joinable()) {
            mUDPThread.detach();
        }
        Application::SetSubsystemStatus("UDPNetwork", Application::Status::Shutdown);
    });
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("TCPNetwork", Application::Status::ShuttingDown);
        if (mTCPThread.joinable()) {
            mTCPThread.detach();
        }
        Application::SetSubsystemStatus("TCPNetwork", Application::Status::Shutdown);
    });
    mTCPThread = std::thread(&TNetwork::TCPServerMain, this);
    mUDPThread = std::thread(&TNetwork::UDPServerMain, this);
}

void TNetwork::UDPServerMain() {
    RegisterThread("UDPServer");

    boost::system::error_code ec;
    auto address = boost::asio::ip::make_address(Application::Settings.getAsString(Settings::Key::General_IP), ec);

    if (ec) {
        beammp_errorf("Failed to parse IP: {}", ec.message());
        Application::GracefullyShutdown();
    }

    boost::asio::ip::udp::endpoint UdpListenEndpoint(address, Application::Settings.getAsInt(Settings::Key::General_Port));

    mUDPSock.open(UdpListenEndpoint.protocol(), ec);
    if (ec) {
        beammp_error("open() failed: " + ec.message());
        std::this_thread::sleep_for(std::chrono::seconds(5));
        Application::GracefullyShutdown();
    }
    // set IP_V6ONLY to false to allow both v4 and v6
    boost::asio::ip::v6_only option(false);
    mUDPSock.set_option(option, ec);
    if (ec) {
        beammp_warnf("Failed to unset IP_V6ONLY on UDP, only IPv6 will work: {}", ec.message());
    }
    mUDPSock.bind(UdpListenEndpoint, ec);
    if (ec) {
        beammp_error("bind() failed: " + ec.message());
        std::this_thread::sleep_for(std::chrono::seconds(5));
        Application::GracefullyShutdown();
    }
    Application::SetSubsystemStatus("UDPNetwork", Application::Status::Good);
    beammp_info(("Vehicle data network online on port ") + std::to_string(UdpListenEndpoint.port()) + (" with a Max of ")
        + std::to_string(Application::Settings.getAsInt(Settings::Key::General_MaxPlayers)) + (" Clients"));
    while (!Application::IsShuttingDown()) {
        try {
            boost::asio::ip::udp::endpoint remote_client_ep { };
            std::vector<uint8_t> Data = UDPRcvFromClient(remote_client_ep);
            if (Data.empty()) {
                continue;
            }
            if (Data.size() == 1 && Data.at(0) == 'P') {
                mUDPSock.send_to(boost::asio::const_buffer("P", 1), remote_client_ep, { }, ec);
                // ignore errors
                (void)ec;
                continue;
            }
            auto Pos = std::find(Data.begin(), Data.end(), ':');
            if (Pos > Data.begin() + 2) {
                continue;
            }
            uint8_t ID = uint8_t(Data.at(0)) - 1;
            mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
                std::shared_ptr<TClient> Client;
                {
                    ReadLock Lock(mServer.GetClientMutex());
                    if (auto Locked = ClientPtr.lock()) {
                        Client = std::move(Locked);
                    } else
                        return true;
                }

                if (Client->GetID() == ID) {
                    if (Client->GetUDPAddr() == boost::asio::ip::udp::endpoint { } && !Client->IsUDPConnected() && !Client->GetMagic().empty()) {
                        if (Data.size() != 66) {
                            beammp_debugf("Invalid size for UDP value. IP: {} ID: {}", remote_client_ep.address().to_string(), ID);
                            return false;
                        }

                        const std::vector Magic(Data.begin() + 2, Data.end());

                        if (Magic != Client->GetMagic()) {
                            beammp_debugf("Invalid value for UDP IP: {} ID: {}", remote_client_ep.address().to_string(), ID);
                            return false;
                        }

                        Client->SetMagic({ });
                        Client->SetUDPAddr(remote_client_ep);
                        Client->SetIsUDPConnected(true);
                        return false;
                    }

                    if (Client->GetUDPAddr() == remote_client_ep) {
                        Data.erase(Data.begin(), Data.begin() + 2);
                        mServer.GlobalParser(ClientPtr, std::move(Data), mPPSMonitor, *this, true);
                    } else {
                        beammp_debugf("Ignored UDP packet for Client {} due to remote address mismatch. Source: {}, Client: {}", ID, remote_client_ep.address().to_string(), Client->GetUDPAddr().address().to_string());
                        return false;
                    }
                }

                return true;
            });
        } catch (const std::exception& e) {
            beammp_warnf("Failed to receive/parse packet via UDP: {}", e.what());
        }
    }
}

void TNetwork::TCPServerMain() {
    RegisterThread("TCPServer");

    boost::system::error_code ec;
    auto address = boost::asio::ip::make_address(Application::Settings.getAsString(Settings::Key::General_IP), ec);
    if (ec) {
        beammp_errorf("Failed to parse IP: {}", ec.message());
        return;
    }

    boost::asio::ip::tcp::endpoint ListenEp(address,
        uint16_t(Application::Settings.getAsInt(Settings::Key::General_Port)));

    boost::asio::ip::tcp::socket Listener(mServer.IoCtx());
    Listener.open(ListenEp.protocol(), ec);
    if (ec) {
        beammp_errorf("Failed to open socket: {}", ec.message());
        return;
    }
    // set IP_V6ONLY to false to allow both v4 and v6
    boost::asio::ip::v6_only option(false);
    Listener.set_option(option, ec);
    if (ec) {
        beammp_warnf("Failed to unset IP_V6ONLY on TCP, only IPv6 will work: {}", ec.message());
    }
#if defined(BEAMMP_FREEBSD)
    beammp_warnf("WARNING: On FreeBSD, for IPv4 to work, you must run `sysctl net.inet6.ip6.v6only=0`!");
    beammp_debugf("This is due to an annoying detail in the *BSDs: In the name of security, unsetting the IPV6_V6ONLY option does not work by default (but does not fail???), as it allows IPv4 mapped IPv6 like ::ffff:127.0.0.1, which they deem a security issue. For more information, see RFC 2553, section 3.7.");
#endif
    socket_base::linger LingerOpt { };
    LingerOpt.enabled(false);
    Listener.set_option(LingerOpt, ec);
    if (ec) {
        beammp_errorf("Failed to set up listening socket to not linger / reuse address. "
                      "This may cause the socket to refuse to bind(). Error: {}",
            ec.message());
    }

    boost::asio::ip::tcp::acceptor Acceptor(mServer.IoCtx(), ListenEp);
    Acceptor.listen(socket_base::max_listen_connections, ec);
    if (ec) {
        beammp_errorf("listen() failed, which is needed for the server to operate. "
                      "Shutting down. Error: {}",
            ec.message());
        Application::GracefullyShutdown();
    }
    Application::SetSubsystemStatus("TCPNetwork", Application::Status::Good);
    beammp_infof("Listening on {0} port {1}", ListenEp.address().to_string(), static_cast<uint16_t>(ListenEp.port()));
    beammp_info("Vehicle event network online");
    do {
        try {
            if (Application::IsShuttingDown()) {
                beammp_debug("shutdown during TCP wait for accept loop");
                break;
            }
            boost::asio::ip::tcp::socket ClientSocket = Acceptor.accept(ec);
            if (ec) {
                beammp_errorf("Failed to accept() new client: {}", ec.message());
                continue;
            }
            boost::asio::ip::tcp::endpoint ClientEp = ClientSocket.remote_endpoint(ec);
            if (ec) {
                beammp_errorf("Accepted socket but failed to query remote endpoint for IP address: {}", ec.message());
                continue;
            }
            std::string ClientIP = ClientEp.address().to_string();
            auto MaybeGuard = mConnectionLimiter.TryAcquire(ClientIP);
            if (!MaybeGuard.has_value()) {
                beammp_debugf("Connection rejected for {} due to the global or concurrent connection limit", ClientIP);
                continue;
            }
            // move-swap to avoid copy ctor (deleted)
            auto Guard = std::move(MaybeGuard.value());
            TConnection Conn { std::move(ClientSocket), ClientEp };
            std::thread ID(&TNetwork::Identify, this, std::move(Conn), std::move(Guard));
            ID.detach(); // TODO: Add to a queue and attempt to join periodically
        } catch (const std::exception& e) {
            beammp_errorf("Exception in accept routine: {}", e.what());
        }
    } while (!Application::IsShuttingDown());
}

#undef GetObject // Fixes Windows

#include "Json.h"
namespace json = rapidjson;

void TNetwork::Identify(TConnection&& RawConnection, TConnectionLimiter::TGuard&& Guard) {
    RegisterThreadAuto();
    char Code;

    boost::system::error_code ec = ReadWithTimeout(RawConnection, &Code, 1, std::chrono::seconds(READ_TIMEOUT_S));
    if (ec) {
        // TODO: is this right?!
        beammp_debug("Error occured reading code");
        RawConnection.Socket.shutdown(socket_base::shutdown_both, ec);
        // TODO: is this right too?
        RawConnection.Socket.close(ec);
        return;
    }
    std::shared_ptr<TClient> Client { nullptr };
    try {
        if (Code == 'C') {
            Client = Authentication(std::move(RawConnection));
        } else if (Code == 'D') {
            beammp_errorf("Old download packet detected - the client is wildly out of date, this will be ignored");
            return;
        } else if (Code == 'P') {
            boost::asio::write(RawConnection.Socket, boost::asio::buffer("P"), ec);
            return;
        } else if (Code == 'I') {
            const std::string Data = Application::Settings.getAsBool(Settings::Key::General_InformationPacket) ? THeartbeatThread::lastCall : "";

            const auto Size = static_cast<int32_t>(Data.size());
            std::vector<uint8_t> ToSend;
            ToSend.resize(Data.size() + sizeof(Size));
            std::memcpy(ToSend.data(), &Size, sizeof(Size));
            std::memcpy(ToSend.data() + sizeof(Size), Data.data(), Data.size());

            boost::system::error_code ec;
            boost::asio::write(RawConnection.Socket, boost::asio::buffer(ToSend), ec);
        } else {
            beammp_errorf("Invalid code got in Identify: '{}'", Code);
        }
    } catch (const std::exception& e) {
        beammp_errorf("Error during handling of code {} - client left in invalid state, closing socket: {}", Code, e.what());
        boost::system::error_code ec;
        RawConnection.Socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
        if (ec) {
            beammp_debugf("Failed to shutdown client socket: {}", ec.message());
        }
        RawConnection.Socket.close(ec);
        if (ec) {
            beammp_debugf("Failed to close client socket: {}", ec.message());
        }
    }
}

std::string HashPassword(const std::string& str) {
    std::stringstream ret;
    unsigned char* hash = SHA256(reinterpret_cast<const unsigned char*>(str.c_str()), str.length(), nullptr);
    for (int i = 0; i < 32; i++) {
        ret << std::hex << static_cast<int>(hash[i]);
    }
    return ret.str();
}

std::shared_ptr<TClient> TNetwork::Authentication(TConnection&& RawConnection) {
    auto Client = CreateClient(std::move(RawConnection.Socket));
    std::string ip = "";
    if (RawConnection.SockAddr.address().to_v6().is_v4_mapped()) {
        ip = boost::asio::ip::make_address_v4(ip::v4_mapped_t::v4_mapped, RawConnection.SockAddr.address().to_v6()).to_string();
    } else {
        ip = RawConnection.SockAddr.address().to_string();
    }
    Client->SetIdentifier("ip", ip);
    beammp_tracef("This thread is ip {} ({})", ip, RawConnection.SockAddr.address().to_v6().is_v4_mapped() ? "IPv4 mapped IPv6" : "IPv6");

    if (Application::GetSubsystemStatuses().at("Main") == Application::Status::Starting) {
        ClientKick(*Client, "The server is still starting, please try joining again later.");
        return nullptr;
    }

    beammp_info("Identifying new ClientConnection...");

    auto Data = TCPRcv(*Client, true);
    if (Data.empty()) {
        beammp_debug("Authentication failed: did not receive version packet");
        ClientKick(*Client, "Connection closed during version handshake");
        return nullptr;
    }

    constexpr std::string_view VC = "VC";
    if (Data.size() > 3 && std::equal(Data.begin(), Data.begin() + VC.size(), VC.begin(), VC.end())) {
        std::string ClientVersionStr(reinterpret_cast<const char*>(Data.data() + 2), Data.size() - 2);
        Version ClientVersion = Application::VersionStrToInts(ClientVersionStr + ".0");
        Version MinClientVersion = Application::ClientMinimumVersion();
        if (Application::IsOutdated(ClientVersion, MinClientVersion)) {
            beammp_errorf("Client tried to connect with version '{}', but only versions >= {} are allowed",
                ClientVersion.AsString(), MinClientVersion.AsString());
            ClientKick(*Client, fmt::format("Outdated version, launcher version >={} required to join!", MinClientVersion.AsString()));
            return nullptr;
        }
    } else {
        ClientKick(*Client, fmt::format("Invalid version header: '{}' ({})", std::string(reinterpret_cast<const char*>(Data.data()), Data.size()), Data.size()));
        return nullptr;
    }

    if (!TCPSend(*Client, StringToVector("A"))) { // changed to A for Accepted version
        // TODO: handle
    }

    Data = TCPRcv(*Client, true);
    if (Data.empty()) {
        beammp_debug("Authentication failed: did not receive auth key packet");
        ClientKick(*Client, "Connection closed during authentication");
        return nullptr;
    }

    if (Data.size() > 50) {
        ClientKick(*Client, "Invalid Key (too long)!");
        return nullptr;
    }

    std::string Key(reinterpret_cast<const char*>(Data.data()), Data.size());
    if (Key.empty()) {
        ClientKick(*Client, "Invalid Key (empty)!");
        return nullptr;
    }
    std::string AuthKey = Application::Settings.getAsString(Settings::Key::General_AuthKey);
    std::string ClientIp = Client->GetIdentifiers().at("ip");

    nlohmann::json AuthReq { };
    std::string AuthResStr { };
    try {
        AuthReq = nlohmann::json {
            { "key", Key },
            { "auth_key", AuthKey },
            { "client_ip", ClientIp }
        };

        auto Target = "/pkToUser";

        unsigned int ResponseCode = 0;
        AuthResStr = Http::POST(Application::GetBackendUrlForAuth() + Target, AuthReq.dump(), "application/json", &ResponseCode);

    } catch (const std::exception& e) {
        beammp_debugf("Invalid json sent by client, kicking: {}", e.what());
        ClientKick(*Client, "Invalid Key (invalid UTF8 string)!");
        return nullptr;
    }

    beammp_debug("Response from authentication backend: " + AuthResStr);

    try {
        nlohmann::json AuthRes = nlohmann::json::parse(AuthResStr);

        if (AuthRes["username"].is_string() && AuthRes["username"].size() > 0 && AuthRes["roles"].is_string()
            && AuthRes["guest"].is_boolean() && AuthRes["identifiers"].is_array()) {

            Client->SetName(AuthRes["username"]);
            Client->SetRoles(AuthRes["roles"]);
            Client->SetIsGuest(AuthRes["guest"]);
            for (const auto& ID : AuthRes["identifiers"]) {
                auto Raw = std::string(ID);
                auto SepIndex = Raw.find(':');
                Client->SetIdentifier(Raw.substr(0, SepIndex), Raw.substr(SepIndex + 1));
            }
        } else {
            beammp_error("Invalid authentication data received from authentication backend");
            ClientKick(*Client, "Invalid authentication data!");
            return nullptr;
        }
    } catch (const std::exception& e) {
        beammp_errorf("Client sent invalid key. Error was: {}", e.what());
        // TODO: we should really clarify that this was a backend response or parsing error
        ClientKick(*Client, "Invalid key! Please restart your game.");
        return nullptr;
    }

    beammp_debug("Name -> " + Client->GetName() + ", Guest -> " + std::to_string(Client->IsGuest()) + ", Roles -> " + Client->GetRoles());
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        std::shared_ptr<TClient> Cl;
        {
            ReadLock Lock(mServer.GetClientMutex());
            if (auto Locked = ClientPtr.lock()) {
                Cl = std::move(Locked);
            } else
                return true;
        }
        if (Cl->GetName() == Client->GetName() && Cl->IsGuest() == Client->IsGuest()) {
            DisconnectClient(Cl, "Stale Client (not a real player)");
            return false;
        }

        return true;
    });

    auto Futures = LuaAPI::MP::Engine->TriggerEvent("onPlayerAuth", "", Client->GetName(), Client->GetRoles(), Client->IsGuest(), Client->GetIdentifiers());
    TLuaEngine::WaitForAll(Futures);
    bool NotAllowed = false;
    bool BypassLimit = false;

    for (const auto& Result : Futures) {
        auto Snapshot = Result->GetDetachedSnapshot();
        if (!Snapshot.Error) {
            const int* MaybeInt = std::get_if<int>(&Snapshot.Result.V);
            if (MaybeInt != nullptr) {
                auto Res = *MaybeInt;

                if (Res == 1) {
                    NotAllowed = true;
                    break;
                } else if (Res == 2) {
                    BypassLimit = true;
                }
            }
        }
    }
    std::string Reason;
    bool NotAllowedWithReason = std::any_of(Futures.begin(), Futures.end(),
        [&Reason](const std::shared_ptr<TLuaResult>& Result) -> bool {
            auto Snapshot = Result->GetDetachedSnapshot();
            if (!Snapshot.Error) {
                const std::string* MaybeStr = std::get_if<std::string>(&Snapshot.Result.V);
                if (MaybeStr != nullptr) {
                    Reason = *MaybeStr;
                    return true;
                }
            }
            return false;
        });

    if (!NotAllowedWithReason && !Application::Settings.getAsBool(Settings::Key::General_AllowGuests) && Client->IsGuest()) { //! NotAllowedWithReason because this message has the lowest priority
        NotAllowedWithReason = true;
        Reason = "No guests are allowed on this server! To join, sign up at: forum.beammp.com.";
    }

    if (!NotAllowed && !NotAllowedWithReason && mServer.ClientCount() >= size_t(Application::Settings.getAsInt(Settings::Key::General_MaxPlayers)) && !BypassLimit) {
        NotAllowedWithReason = true;
        Reason = "Server full!";
    }

    if (NotAllowedWithReason) {
        ClientKick(*Client, Reason);
    } else if (NotAllowed) {
        ClientKick(*Client, "you are not allowed on the server!");
    }

    auto PostFutures = LuaAPI::MP::Engine->TriggerEvent("postPlayerAuth", "", NotAllowed || NotAllowedWithReason, Reason, Client->GetName(), Client->GetRoles(), Client->IsGuest(), Client->GetIdentifiers());
    // the post event is not cancellable so we dont wait for it
    LuaAPI::MP::Engine->ReportErrors(PostFutures);

    if (!NotAllowed && !NotAllowedWithReason) {
        beammp_info("Identification success");
        mServer.InsertClient(Client);
        TCPClient(Client);
    }

    return Client;
}

std::shared_ptr<TClient> TNetwork::CreateClient(boost::asio::ip::tcp::socket&& TCPSock) {
    auto c = std::make_shared<TClient>(mServer, std::move(TCPSock));
    return c;
}

bool TNetwork::TCPSend(TClient& c, const std::vector<uint8_t>& Data, bool IsSync) {
    if (!IsSync) {
        if (c.IsSyncing()) {
            if (!Data.empty()) {
                if (Data.at(0) == 'O' || Data.at(0) == 'A' || Data.at(0) == 'C' || Data.at(0) == 'E') {
                    c.EnqueuePacket(Data);
                }
            }
            return true;
        }
    }

    auto& Sock = c.GetTCPSock();

    /*
     * our TCP protocol sends a header of 4 bytes, followed by the data.
     *
     *  [][][][][][]...[]
     *  ^------^^---...-^
     *    size    data
     */

    const auto Size = int32_t(Data.size());
    std::vector<uint8_t> ToSend;
    ToSend.resize(Data.size() + sizeof(Size));
    std::memcpy(ToSend.data(), &Size, sizeof(Size));
    std::memcpy(ToSend.data() + sizeof(Size), Data.data(), Data.size());
    boost::system::error_code ec;
    boost::asio::write(Sock, boost::asio::buffer(ToSend), ec);
    if (ec) {
        beammp_debugf("write(): {}", ec.message());
        DisconnectClient(c, "write() failed");
        return false;
    }
    c.UpdatePingTime();
    return true;
}

std::vector<uint8_t> TNetwork::TCPRcv(TClient& c, bool WithTimeout) {
    if (c.IsDisconnected()) {
        beammp_error("Client disconnected, cancelling TCPRcv");
        return { };
    }

    int32_t Header { };
    auto& Sock = c.GetTCPSock();

    boost::system::error_code ec;
    std::array<uint8_t, sizeof(Header)> HeaderData;
    if (WithTimeout) {
        ec = ReadWithTimeout(Sock, HeaderData.data(), HeaderData.size(), std::chrono::seconds(READ_TIMEOUT_S));
    } else {
        boost::asio::read(Sock, boost::asio::buffer(HeaderData), ec);
    }
    if (ec) {
        // TODO: handle this case (read failed)
        beammp_debugf("TCPRcv: Reading header failed: {}", ec.message());
        return { };
    }
    Header = *reinterpret_cast<int32_t*>(HeaderData.data());

    if (Header < 0) {
        ClientKick(c, "Invalid packet - header negative");
        beammp_errorf("Client {} send negative TCP header, ignoring packet", c.GetID());
        return { };
    }

    std::vector<uint8_t> Data;
    // TODO: This is arbitrary, this needs to be handled another way
    bool isUnauthenticated = c.GetName().empty();
    int32_t maxHeaderSize = isUnauthenticated ? 4096 : int32_t(100 * MB);
    if (Header < maxHeaderSize) {
        Data.resize(Header);
    } else {
        ClientKick(c, "Header size limit exceeded");
        beammp_warn("Client " + c.GetName() + " (" + std::to_string(c.GetID()) + ") sent header larger than expected - assuming malicious intent and disconnecting the client.");
        return { };
    }
    std::size_t N = 0;
    if (WithTimeout) {
        if (!Data.empty()) {
            ec = ReadWithTimeout(Sock, Data.data(), Data.size(), std::chrono::seconds(READ_TIMEOUT_S));
            if (!ec) {
                N = Data.size();
            }
        }
    } else {
        N = boost::asio::read(Sock, boost::asio::buffer(Data), ec);
    }
    if (ec) {
        // TODO: handle this case properly
        beammp_debugf("TCPRcv: Reading data failed: {}", ec.message());
        return { };
    }

    if (N != Header) {
        beammp_errorf("Expected to read {} bytes, instead got {}", Header, N);
    }

    constexpr std::string_view ABG = "ABG:";
    if (Data.size() >= ABG.size() && std::equal(Data.begin(), Data.begin() + ABG.size(), ABG.begin(), ABG.end())) {
        Data.erase(Data.begin(), Data.begin() + ABG.size());
        try {
            return DeComp(Data);
        } catch (const InvalidDataError&) {
            beammp_errorf("Failed to decompress packet from a client. The receive failed and the client may be disconnected as a result");
            // return empty -> error
            return std::vector<uint8_t>();
        } catch (const std::runtime_error& e) {
            beammp_errorf("Failed to decompress packet from a client: {}. The server may be out of RAM! The receive failed and the client may be disconnected as a result", e.what());
            // return empty -> error
            return std::vector<uint8_t>();
        }
    } else {
        return Data;
    }
}

void TNetwork::ClientKick(TClient& c, const std::string& R) {
    beammp_info("Client kicked: " + R);
    if (!TCPSend(c, StringToVector("K" + R))) {
        beammp_debugf("tried to kick player '{}' (id {}), but was already disconnected", c.GetName(), c.GetID());
    }
    DisconnectClient(c, "Kicked");
}

void TNetwork::DisconnectClient(const std::weak_ptr<TClient>& c, const std::string& R) {
    if (auto locked = c.lock()) {
        DisconnectClient(*locked, R);
    } else {
        beammp_debugf("Tried to disconnect a non existant client with reason: {}", R);
    }
}

void TNetwork::DisconnectClient(TClient& c, const std::string& R) {
    // Keep this unconditional; TClient::Disconnect() is the single-winner guard.
    (void)c.Disconnect(R);
}

void TNetwork::Looper(const std::weak_ptr<TClient>& c) {
    RegisterThreadAuto();
    while (true) {
        auto Client = c.lock();
        if (!Client) {
            break;
        }
        if (Client->IsDisconnected()) {
            beammp_debug("client is disconnected, breaking client loop");
            break;
        }
        if (!Client->IsSyncing() && Client->IsSynced() && Client->MissedPacketQueueSize() != 0) {
            // debug("sending " + std::to_string(Client->MissedPacketQueueSize()) + " queued packets");
            while (Client->MissedPacketQueueSize() > 0) {
                std::vector<uint8_t> QData { };
                { // locked context
                    std::unique_lock lock(Client->MissedPacketQueueMutex());
                    if (Client->MissedPacketQueueSize() <= 0) {
                        break;
                    }
                    QData = Client->MissedPacketQueue().front();
                    Client->MissedPacketQueue().pop();
                } // end locked context
                // beammp_debug("sending a missed packet: " + QData);
                if (!TCPSend(*Client, QData, true)) {
                    DisconnectClient(Client, "Failed to TCPSend while clearing the missed packet queue");
                    std::unique_lock lock(Client->MissedPacketQueueMutex());
                    while (!Client->MissedPacketQueue().empty()) {
                        Client->MissedPacketQueue().pop();
                    }
                    break;
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void TNetwork::TCPClient(const std::weak_ptr<TClient>& c) {
    if (auto Client = c.lock()) {
        if (Client->IsDisconnected()) {
            mServer.RemoveClient(c);
            return;
        }
    } else {
        mServer.RemoveClient(c);
        return;
    }
    OnConnect(c);
    if (auto Client = c.lock()) {
        RegisterThread("(" + std::to_string(Client->GetID()) + ") \"" + Client->GetName() + "\"");
    } else {
        return;
    }

    std::jthread QueueSync(&TNetwork::Looper, this, c);

    while (true) {
        auto Client = c.lock();
        if (!Client) {
            break;
        }
        if (Client->IsDisconnected()) {
            beammp_debug("client status < 0, breaking client loop");
            break;
        }

        auto res = TCPRcv(*Client);
        if (res.empty()) {
            beammp_debug("TCPRcv empty");
            DisconnectClient(Client, "TCPRcv failed");
            break;
        }
        try {
            mServer.GlobalParser(c, std::move(res), mPPSMonitor, *this, false);
        } catch (const std::exception& e) {
            beammp_warnf("Failed to receive/parse packet via TCP from client {}: {}", Client->GetID(), e.what());
            DisconnectClient(Client, "Failed to parse packet");
            break;
        }
    }

    if (auto Client = c.lock()) {
        OnDisconnect(c);
    } else {
        beammp_warn("client expired in TCPClient, should never happen");
    }
}

void TNetwork::UpdatePlayer(TClient& Client) {
    std::string Packet = ("Ss") + std::to_string(mServer.ClientCount()) + "/" + std::to_string(Application::Settings.getAsInt(Settings::Key::General_MaxPlayers)) + ":";
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        ReadLock Lock(mServer.GetClientMutex());
        if (auto c = ClientPtr.lock()) {
            Packet += c->GetName() + ",";
        }
        return true;
    });
    Packet = Packet.substr(0, Packet.length() - 1);
    Client.EnqueuePacket(StringToVector(Packet));
    //(void)Respond(Client, Packet, true);
}

static boost::system::error_code ReadSocketWithTimeout(
    boost::asio::ip::tcp::socket& socket,
    void* buffer,
    std::size_t length,
    std::chrono::steady_clock::duration timeout);

boost::system::error_code TNetwork::ReadWithTimeout(TConnection& Connection, void* Buf, size_t Len, std::chrono::steady_clock::duration Timeout) {
    return ReadWithTimeout(Connection.Socket, Buf, Len, Timeout);
}

boost::system::error_code TNetwork::ReadWithTimeout(boost::asio::ip::tcp::socket& Socket, void* Buf, size_t Len, std::chrono::steady_clock::duration Timeout) {
    return ReadSocketWithTimeout(Socket, Buf, Len, Timeout);
}

static boost::system::error_code ReadSocketWithTimeout(
    boost::asio::ip::tcp::socket& Socket,
    void* Buffer,
    std::size_t Length,
    std::chrono::steady_clock::duration Timeout) {
    namespace asio = boost::asio;
    using boost::system::error_code;

    struct TTimeoutState {
        explicit TTimeoutState(const boost::asio::any_io_executor& Executor)
            : Timer(Executor) { }

        asio::steady_timer Timer;
        std::promise<std::pair<error_code, std::size_t>> Promise;
        std::atomic_bool Completed { false };
    };

    auto State = std::make_shared<TTimeoutState>(Socket.get_executor());
    auto Future = State->Promise.get_future();

    asio::async_read(
        Socket,
        asio::buffer(Buffer, Length),
        [State](error_code ec, std::size_t n) {
            if (!State->Completed.exchange(true)) {
                State->Timer.cancel();
                State->Promise.set_value({ ec, n });
            }
        });

    State->Timer.expires_after(Timeout);
    State->Timer.async_wait(
        [State, &Socket](error_code ec) {
            if (ec == asio::error::operation_aborted)
                return;

            if (!State->Completed.exchange(true)) {
                error_code IgnoredEc;
                Socket.cancel(IgnoredEc);
                State->Promise.set_value({ asio::error::timed_out, 0 });
            }
        });

    auto [ec, NRead] = Future.get();
    return ec;
}

TEST_CASE("ReadSocketWithTimeout returns timed_out when peer sends no data") {
    TIoPollThread TimerThread;
    boost::system::error_code Ec;
    ip::tcp::socket ClientSocket(TimerThread.IoCtx());
    ip::tcp::socket ServerSocket(TimerThread.IoCtx());
    OpenLoopbackSocketPair(TimerThread.IoCtx(), ClientSocket, ServerSocket, Ec);
    REQUIRE(!Ec);

    uint8_t ReadByte = 0;
    const auto ReadEc = ReadSocketWithTimeout(ServerSocket, &ReadByte, 1, std::chrono::milliseconds(50));

    CHECK(ReadEc == error::timed_out);
}

TEST_CASE("ReadSocketWithTimeout reads small payload") {
    TIoPollThread TimerThread;
    boost::system::error_code Ec;
    ip::tcp::socket ClientSocket(TimerThread.IoCtx());
    ip::tcp::socket ServerSocket(TimerThread.IoCtx());
    OpenLoopbackSocketPair(TimerThread.IoCtx(), ClientSocket, ServerSocket, Ec);
    REQUIRE(!Ec);

    const std::array<uint8_t, 2> Sent { 'O', 'K' };
    boost::asio::write(ClientSocket, boost::asio::buffer(Sent), Ec);
    REQUIRE(!Ec);
    std::array<uint8_t, 2> Received { };
    const auto ReadEc = ReadSocketWithTimeout(ServerSocket, Received.data(), Received.size(), std::chrono::milliseconds(200));

    CHECK(!ReadEc);
    CHECK(Received == Sent);
}

TEST_CASE("ReadSocketWithTimeout reads large payload") {
    TIoPollThread TimerThread;
    boost::system::error_code Ec;
    ip::tcp::socket ClientSocket(TimerThread.IoCtx());
    ip::tcp::socket ServerSocket(TimerThread.IoCtx());
    OpenLoopbackSocketPair(TimerThread.IoCtx(), ClientSocket, ServerSocket, Ec);
    REQUIRE(!Ec);

    constexpr size_t PacketSize = 2 * 1024 * 1024;
    std::vector<uint8_t> Sent(PacketSize, uint8_t(0x7A));
    boost::asio::write(ClientSocket, boost::asio::buffer(Sent), Ec);
    REQUIRE(!Ec);
    std::vector<uint8_t> Received(PacketSize);
    const auto ReadEc = ReadSocketWithTimeout(ServerSocket, Received.data(), Received.size(), std::chrono::seconds(2));

    CHECK(!ReadEc);
    CHECK(Received == Sent);
}

TEST_CASE("ReadSocketWithTimeout can timeout then retry successfully") {
    TIoPollThread TimerThread;
    boost::system::error_code Ec;
    ip::tcp::socket ClientSocket(TimerThread.IoCtx());
    ip::tcp::socket ServerSocket(TimerThread.IoCtx());
    OpenLoopbackSocketPair(TimerThread.IoCtx(), ClientSocket, ServerSocket, Ec);
    REQUIRE(!Ec);

    uint8_t Received = 0;
    CHECK(ReadSocketWithTimeout(ServerSocket, &Received, 1, std::chrono::milliseconds(20)) == error::timed_out);

    const uint8_t Sent = 0x42;
    boost::asio::write(ClientSocket, boost::asio::buffer(&Sent, 1), Ec);
    REQUIRE(!Ec);
    const auto ReadEc = ReadSocketWithTimeout(ServerSocket, &Received, 1, std::chrono::milliseconds(200));

    CHECK(!ReadEc);
    CHECK(Received == Sent);
}

void TNetwork::OnDisconnect(const std::weak_ptr<TClient>& ClientPtr) {
    auto LockedClientPtr = ClientPtr.lock();
    if (!LockedClientPtr) {
        beammp_warn("Client expired in OnDisconnect, this is unexpected");
        return;
    }
    TClient& c = *LockedClientPtr;
    beammp_info(c.GetName() + (" Connection Terminated"));
    std::string Packet;
    TClient::TSetOfVehicleData VehicleData;
    { // Vehicle Data Lock Scope
        auto LockedData = c.GetAllCars();
        VehicleData = *LockedData.VehicleData;
    } // End Vehicle Data Lock Scope
    for (auto& v : VehicleData) {
        LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onVehicleDeleted", "", c.GetID(), v.ID()));
        Packet = "Od:" + std::to_string(c.GetID()) + "-" + std::to_string(v.ID());
        SendToAll(&c, StringToVector(Packet), false, true);
    }
    Packet = ("L") + c.GetName() + (" left the server!");
    SendToAll(&c, StringToVector(Packet), false, true);
    Packet.clear();
    auto Futures = LuaAPI::MP::Engine->TriggerEvent("onPlayerDisconnect", "", c.GetID());
    LuaAPI::MP::Engine->WaitForAll(Futures);
    DisconnectClient(c, "Already Disconnected (OnDisconnect)");
    mServer.RemoveClient(ClientPtr);
}

int TNetwork::OpenID() {
    std::unique_lock OpenIDLock(mOpenIDMutex);
    int ID = 0;
    bool found;
    do {
        found = true;
        mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
            ReadLock Lock(mServer.GetClientMutex());
            if (auto c = ClientPtr.lock()) {
                if (c->GetID() == ID) {
                    found = false;
                    ID++;
                }
            }
            return true;
        });
    } while (!found);
    return ID;
}

void TNetwork::OnConnect(const std::weak_ptr<TClient>& c) {
    auto LockedClient = c.lock();
    if (!LockedClient) {
        return;
    }
    beammp_info("Client connected");
    LockedClient->SetID(OpenID());
    beammp_info("Assigned ID " + std::to_string(LockedClient->GetID()) + " to " + LockedClient->GetName());
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerConnecting", "", LockedClient->GetID()));
    SyncResources(*LockedClient);
    if (LockedClient->IsDisconnected())
        return;
    std::vector<unsigned char> buf(64);
    int ret = RAND_bytes(buf.data(), buf.size());
    if (ret != 1) {
        unsigned long error = ERR_get_error();
        beammp_errorf("RAND_bytes failed with error code {}", error);
        beammp_assert(ret != 1);
        return;
    }

    LockedClient->SetMagic(buf);
    buf.insert(buf.begin(), 'U');
    (void)Respond(*LockedClient, buf, true);
    (void)Respond(*LockedClient, StringToVector("M" + Application::Settings.getAsString(Settings::Key::General_Map)), true); // Send the Map on connect
    beammp_info(LockedClient->GetName() + " : Connected");
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerJoining", "", LockedClient->GetID()));
}

void TNetwork::SyncResources(TClient& c) {
    if (!TCPSend(c, StringToVector("P" + std::to_string(c.GetID())))) {
        // TODO handle
    }
    std::vector<uint8_t> Data;
    while (!c.IsDisconnected()) {
        Data = TCPRcv(c);
        if (Data.empty()) {
            DisconnectClient(c, "TCPRcv failed during resource sync");
            break;
        }
        constexpr std::string_view Done = "Done";
        if (std::equal(Data.begin(), Data.end(), Done.begin(), Done.end()))
            break;
        Parse(c, Data);
    }
}

void TNetwork::Parse(TClient& c, const std::vector<uint8_t>& Packet) {
    if (Packet.empty())
        return;
    char Code = Packet.at(0), SubCode = 0;
    if (Packet.size() > 1)
        SubCode = Packet.at(1);
    switch (Code) {
    case 'f':
        SendFile(c, std::string(reinterpret_cast<const char*>(Packet.data() + 1), Packet.size() - 1));
        return;
    case 'S':
        if (SubCode == 'R') {
            beammp_debug("Sending Mod Info");
            std::string ToSend = mResourceManager.GetMods().dump();
            beammp_debugf("Mod Info: {}", ToSend);
            if (!TCPSend(c, StringToVector(ToSend))) {
                ClientKick(c, "TCP Send 'SY' failed");
                return;
            }
        }
        return;
    default:
        return;
    }
}

void TNetwork::SendFile(TClient& c, const std::string& UnsafeName) {
    if (!fs::path(UnsafeName).has_filename()) {
        if (!TCPSend(c, StringToVector("CO"))) {
            // TODO: handle
        }
        beammp_warn("File " + UnsafeName + " is not a file!");
        return;
    }
    auto FileName = fs::path(UnsafeName).filename().string();

    for (auto mod : mResourceManager.GetMods()) {
        if (mod["file_name"].get<std::string>() == FileName && mod["protected"] == true) {
            beammp_warn("Client tried to access protected file " + UnsafeName);
            DisconnectClient(c, "Mod is protected thus cannot be downloaded");
            return;
        }
    }

    FileName = Application::Settings.getAsString(Settings::Key::General_ResourceFolder) + "/Client/" + FileName;

    if (!std::filesystem::exists(FileName)) {
        if (!TCPSend(c, StringToVector("CO"))) {
            // TODO: handle
        }
        beammp_warn("File " + UnsafeName + " could not be accessed!");
        return;
    }

    if (!TCPSend(c, StringToVector("AG"))) {
        // TODO: handle
    }

    size_t Size = size_t(std::filesystem::file_size(FileName));

    SendFileToClient(c, Size, FileName);
}

#if defined(BEAMMP_LINUX)
#include <cerrno>
#include <cstring>
#include <signal.h>
#include <sys/sendfile.h>
#include <unistd.h>
#endif
void TNetwork::SendFileToClient(TClient& c, size_t Size, const std::string& Name) {
    TScopedTimer timer(fmt::format("Download of '{}' for client {}", Name, c.GetID()));
#if defined(BEAMMP_LINUX)
    signal(SIGPIPE, SIG_IGN);
    // on linux, we can use sendfile(2)!
    int fd = ::open(Name.c_str(), O_RDONLY);
    if (fd < 0) {
        beammp_errorf("Failed to open mod '{}' for sending, error: {}", Name, std::strerror(errno));
        return;
    }
    // native handle, needed in order to make native syscalls with it
    int socket = c.GetTCPSock().native_handle();

    const auto ToSendTotal = Size;
    size_t TotalSent = 0;
    while (TotalSent < ToSendTotal) {
        off_t SysOffset = off_t(TotalSent);
        const ssize_t SentNow = sendfile(socket, fd, &SysOffset, ToSendTotal - TotalSent);
        if (SentNow > 0) {
            TotalSent += size_t(SentNow);
            continue;
        }
        if (SentNow == 0) {
            beammp_errorf("Failed to send mod '{}' to client {}: sendfile returned 0 before all bytes were sent", Name, c.GetID());
            ::close(fd);
            DisconnectClient(c, "sendfile returned 0 during mod download");
            return;
        }

        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN
#if EWOULDBLOCK != EAGAIN
            || errno == EWOULDBLOCK
#endif
        ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        beammp_errorf("Failed to send mod '{}' to client {}: {}", Name, c.GetID(), std::strerror(errno));
        ::close(fd);
        DisconnectClient(c, "sendfile failed during mod download");
        return;
    }
    ::close(fd);

#else
    std::ifstream f(Name.c_str(), std::ios::binary);
    uint32_t Split = 125 * MB;
    std::vector<uint8_t> Data;
    if (Size > Split)
        Data.resize(Split);
    else
        Data.resize(Size);
    boost::asio::ip::tcp::socket* TCPSock = &c.GetTCPSock();
    std::streamsize Sent = 0;
    while (!c.IsDisconnected() && Sent < Size) {
        size_t Diff = Size - Sent;
        if (Diff > Split) {
            f.seekg(Sent, std::ios_base::beg);
            f.read(reinterpret_cast<char*>(Data.data()), Split);
            if (!TCPSendRaw(c, *TCPSock, Data.data(), Split)) {
                if (!c.IsDisconnected())
                    DisconnectClient(c, "TCPSendRaw failed in mod download (1)");
                break;
            }
            Sent += Split;
        } else {
            f.seekg(Sent, std::ios_base::beg);
            f.read(reinterpret_cast<char*>(Data.data()), Diff);
            if (!TCPSendRaw(c, *TCPSock, Data.data(), int32_t(Diff))) {
                if (!c.IsDisconnected())
                    DisconnectClient(c, "TCPSendRaw failed in mod download (2)");
                break;
            }
            Sent += Diff;
        }
    }
#endif
}

bool TNetwork::TCPSendRaw(TClient& C, boost::asio::ip::tcp::socket& socket, const uint8_t* Data, size_t Size) {
    boost::system::error_code ec;
    boost::asio::write(socket, boost::asio::buffer(Data, Size), ec);
    if (ec) {
        beammp_errorf("Failed to send raw data to client: {}", ec.message());
        return false;
    }
    C.UpdatePingTime();
    return true;
}

bool TNetwork::SendLarge(TClient& c, std::vector<uint8_t> Data, bool isSync) {
    if (Data.size() > 400) {
        CompressProperly(Data);
    }
    return TCPSend(c, Data, isSync);
}

bool TNetwork::Respond(TClient& c, const std::vector<uint8_t>& MSG, bool Rel, bool isSync) {
    char C = MSG.at(0);
    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E' || compressBound(MSG.size()) > 1024) {
        if (C == 'O' || C == 'T' || MSG.size() > 1000) {
            return SendLarge(c, MSG, isSync);
        } else {
            return TCPSend(c, MSG, isSync);
        }
    } else {
        return UDPSend(c, MSG);
    }
}

bool TNetwork::SyncClient(const std::weak_ptr<TClient>& c) {
    auto LockedClient = c.lock();
    if (!LockedClient) {
        return false;
    }
    if (LockedClient->IsSynced())
        return true;
    // Syncing, later set isSynced
    // after syncing is done, we apply all packets they missed
    if (!Respond(*LockedClient, StringToVector("Sn" + LockedClient->GetName()), true)) {
        return false;
    }
    // ignore error
    (void)SendToAll(LockedClient.get(), StringToVector("JWelcome " + LockedClient->GetName() + "!"), false, true);

    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerJoin", "", LockedClient->GetID()));
    LockedClient->SetIsSyncing(true);
    bool Return = false;
    bool res = true;
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        std::shared_ptr<TClient> client;
        {
            ReadLock Lock(mServer.GetClientMutex());
            if (auto Locked = ClientPtr.lock()) {
                client = std::move(Locked);
            } else
                return true;
        }
        TClient::TSetOfVehicleData VehicleData;
        { // Vehicle Data Lock Scope
            auto LockedData = client->GetAllCars();
            VehicleData = *LockedData.VehicleData;
        } // End Vehicle Data Lock Scope
        if (client != LockedClient) {
            for (auto& v : VehicleData) {
                if (LockedClient->IsDisconnected()) {
                    Return = true;
                    res = false;
                    return false;
                }
                res = Respond(*LockedClient, StringToVector(v.DataAsPacket(client->GetRoles(), client->GetName(), client->GetID())), true, true);
            }
        }

        return true;
    });
    LockedClient->SetIsSyncing(false);
    if (Return) {
        return res;
    }
    LockedClient->SetIsSynced(true);
    beammp_info(LockedClient->GetName() + (" is now synced!"));
    return true;
}

void TNetwork::SendToAll(TClient* c, const std::vector<uint8_t>& Data, bool Self, bool Rel) {
    if (!Self)
        beammp_assert(c);
    char C = Data.at(0);
    bool ret = true;
    mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
        std::shared_ptr<TClient> Client { nullptr };
        {
            ReadLock Lock(mServer.GetClientMutex());
            if (auto Locked = ClientPtr.lock()) {
                Client = std::move(Locked);
            } else {
                return true;
            }
        }
        if (Self || Client.get() != c) {
            if (Client->IsSynced() || Client->IsSyncing()) {
                if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E' || compressBound(Data.size()) > 1024) {
                    if (C == 'O' || C == 'T' || Data.size() > 1000) {
                        if (Data.size() > 400) {
                            auto CompressedData = Data;
                            CompressProperly(CompressedData);
                            Client->EnqueuePacket(CompressedData);
                        } else {
                            Client->EnqueuePacket(Data);
                        }
                        // ret = SendLarge(*Client, Data);
                    } else {
                        Client->EnqueuePacket(Data);
                        // ret = TCPSend(*Client, Data);
                    }
                } else {
                    ret = UDPSend(*Client, Data);
                }
            }
        }
        return true;
    });
    if (!ret) {
        // TODO: handle
    }
    return;
}

bool TNetwork::UDPSend(TClient& Client, std::vector<uint8_t> Data) {
    if (!Client.IsUDPConnected() || Client.IsDisconnected()) {
        // this can happen if we try to send a packet to a client that is either
        // 1. not yet fully connected, or
        // 2. disconnected and not yet fully removed
        // this is fine can can be ignored :^)
        return true;
    }
    const auto Addr = Client.GetUDPAddr();
    if (Data.size() > 400) {
        CompressProperly(Data);
    }
    boost::system::error_code ec;
    mUDPSock.send_to(boost::asio::buffer(Data), Addr, 0, ec);
    if (ec) {
        beammp_debugf("UDP sendto() failed: {}", ec.message());
        if (!Client.IsDisconnected())
            DisconnectClient(Client, "UDP send failed");
        return false;
    }
    return true;
}

std::vector<uint8_t> TNetwork::UDPRcvFromClient(boost::asio::ip::udp::endpoint& ClientEndpoint) {
    std::array<char, 1024> Ret { };
    boost::system::error_code ec;
    const auto Rcv = mUDPSock.receive_from(boost::asio::mutable_buffer(Ret.data(), Ret.size()), ClientEndpoint, 0, ec);
    if (ec) {
        beammp_errorf("UDP recvfrom() failed: {}", ec.message());
        return { };
    }
    beammp_assert(Rcv <= Ret.size());
    return std::vector<uint8_t>(Ret.begin(), Ret.begin() + Rcv);
}
