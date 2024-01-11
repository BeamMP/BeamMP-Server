#include "TNetwork.h"
#include "Client.h"
#include "Common.h"
#include "LuaAPI.h"
#include "TLuaEngine.h"
#include "nlohmann/json.hpp"
#include <CustomAssert.h>
#include <Http.h>
#include <array>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <cstring>

typedef boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO> rcv_timeout_option;

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

TNetwork::TNetwork(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager)
    : mServer(Server)
    , mPPSMonitor(PPSMonitor)
    , mUDPSock(Server.IoCtx())
    , mResourceManager(ResourceManager) {
    Application::SetSubsystemStatus("TCPNetwork", Application::Status::Starting);
    Application::SetSubsystemStatus("UDPNetwork", Application::Status::Starting);
    Application::RegisterShutdownHandler([&] {
        beammp_debug("Kicking all players due to shutdown");
        Server.ForEachClient([&](std::shared_ptr<TClient> Client) -> bool {
            ClientKick(*Client, "Server shutdown");
            return true;
        });
    });
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
    ip::udp::endpoint UdpListenEndpoint(ip::address::from_string("0.0.0.0"), Application::Settings.Port);
    boost::system::error_code ec;
    mUDPSock.open(UdpListenEndpoint.protocol(), ec);
    if (ec) {
        beammp_error("open() failed: " + ec.message());
        std::this_thread::sleep_for(std::chrono::seconds(5));
        Application::GracefullyShutdown();
    }
    mUDPSock.bind(UdpListenEndpoint, ec);
    if (ec) {
        beammp_error("bind() failed: " + ec.message());
        std::this_thread::sleep_for(std::chrono::seconds(5));
        Application::GracefullyShutdown();
    }
    Application::SetSubsystemStatus("UDPNetwork", Application::Status::Good);
    beammp_info(("Vehicle data network online on port ") + std::to_string(Application::Settings.Port) + (" with a Max of ")
        + std::to_string(Application::Settings.MaxPlayers) + (" Clients"));
    while (!Application::IsShuttingDown()) {
        try {
            ip::udp::endpoint client {};
            std::vector<uint8_t> Data = UDPRcvFromClient(client); // Receives any data from Socket
            auto Pos = std::find(Data.begin(), Data.end(), ':');
            if (Data.empty() || Pos > Data.begin() + 2)
                continue;
            uint8_t ID = uint8_t(Data.at(0)) - 1;
            mServer.ForEachClient([&](std::shared_ptr<TClient> Client) -> bool {
                if (Client->ID == ID) {
                    Client->UDPAddress = client;
                    Client->IsConnected = true;
                    Data.erase(Data.begin(), Data.begin() + 2);
                    mServer.GlobalParser(Client, std::move(Data), mPPSMonitor, *this);
                }
                return true;
            });
        } catch (const std::exception& e) {
            beammp_error(("fatal: ") + std::string(e.what()));
        }
    }
}

void TNetwork::TCPServerMain() {
    RegisterThread("TCPServer");

    ip::tcp::endpoint ListenEp(ip::address::from_string("0.0.0.0"), Application::Settings.Port);
    ip::tcp::socket Listener(mServer.IoCtx());
    boost::system::error_code ec;
    Listener.open(ListenEp.protocol(), ec);
    if (ec) {
        beammp_errorf("Failed to open socket: {}", ec.message());
        return;
    }
    socket_base::linger LingerOpt {};
    LingerOpt.enabled(false);
    Listener.set_option(LingerOpt, ec);
    if (ec) {
        beammp_errorf("Failed to set up listening socket to not linger / reuse address. "
                      "This may cause the socket to refuse to bind(). Error: {}",
            ec.message());
    }

    ip::tcp::acceptor Acceptor(mServer.IoCtx(), ListenEp);
    Acceptor.listen(socket_base::max_listen_connections, ec);
    if (ec) {
        beammp_errorf("listen() failed, which is needed for the server to operate. "
                      "Shutting down. Error: {}",
            ec.message());
        Application::GracefullyShutdown();
    }
    Application::SetSubsystemStatus("TCPNetwork", Application::Status::Good);
    beammp_info("Vehicle event network online");
    do {
        try {
            if (Application::IsShuttingDown()) {
                beammp_debug("shutdown during TCP wait for accept loop");
                break;
            }
            ip::tcp::endpoint ClientEp;
            ip::tcp::socket ClientSocket = Acceptor.accept(ClientEp, ec);
            if (ec) {
                beammp_errorf("failed to accept: {}", ec.message());
            }
            TConnection Conn { std::move(ClientSocket), ClientEp };
            std::thread ID(&TNetwork::Identify, this, std::move(Conn));
            ID.detach(); // TODO: Add to a queue and attempt to join periodically
        } catch (const std::exception& e) {
            beammp_error("fatal: " + std::string(e.what()));
        }
    } while (!Application::IsShuttingDown());
}

#undef GetObject // Fixes Windows

#include "Json.h"
namespace json = rapidjson;

void TNetwork::Identify(TConnection&& RawConnection) {
    RegisterThreadAuto();
    char Code;

    boost::system::error_code ec;
    read(RawConnection.Socket, buffer(&Code, 1), ec);
    if (ec) {
        // TODO: is this right?!
        RawConnection.Socket.shutdown(socket_base::shutdown_both, ec);
        return;
    }
    std::shared_ptr<TClient> Client { nullptr };
    try {
        if (Code == 'C') {
            Client = Authentication(std::move(RawConnection));
        } else if (Code == 'D') {
            HandleDownload(std::move(RawConnection));
        } else if (Code == 'P') {
            boost::system::error_code ec;
            write(RawConnection.Socket, buffer("P"), ec);
            return;
        } else {
            beammp_errorf("Invalid code got in Identify: '{}'", Code);
        }
    } catch (const std::exception& e) {
        beammp_errorf("Error during handling of code {}->client left in invalid state, closing socket", Code);
        boost::system::error_code ec;
        RawConnection.Socket.shutdown(socket_base::shutdown_both, ec);
        if (ec) {
            beammp_debugf("Failed to shutdown client socket: {}", ec.message());
        }
        RawConnection.Socket.close(ec);
        if (ec) {
            beammp_debugf("Failed to close client socket: {}", ec.message());
        }
    }
}

void TNetwork::HandleDownload(TConnection&& Conn) {
    char D;
    boost::system::error_code ec;
    read(Conn.Socket, buffer(&D, 1), ec);
    if (ec) {
        Conn.Socket.shutdown(socket_base::shutdown_both, ec);
        // ignore ec
        return;
    }
    auto ID = uint8_t(D);
    auto Client = GetClient(mServer, ID);
    if (Client.has_value()) {
        auto New = Sync<ip::tcp::socket>(std::move(Conn.Socket));
        Client.value()->DownSocket.swap(New);
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
    Client->SetIdentifier("ip", RawConnection.SockAddr.address().to_string());
    beammp_tracef("This thread is ip {}", RawConnection.SockAddr.address().to_string());

    beammp_info("Identifying new ClientConnection...");

    auto Data = TCPRcv(*Client);

    constexpr std::string_view VC = "VC";
    if (Data.size() > 3 && std::equal(Data.begin(), Data.begin() + VC.size(), VC.begin(), VC.end())) {
        std::string ClientVersionStr(reinterpret_cast<const char*>(Data.data() + 2), Data.size() - 2);
        Version ClientVersion = Application::VersionStrToInts(ClientVersionStr + ".0");
        if (ClientVersion.major != Application::ClientMajorVersion()) {
            beammp_errorf("Client tried to connect with version '{}', but only versions '{}.x.x' is allowed",
                ClientVersion.AsString(), Application::ClientMajorVersion());
            ClientKick(*Client, "Outdated Version!");
            return nullptr;
        }
    } else {
        ClientKick(*Client, fmt::format("Invalid version header: '{}' ({})", std::string(reinterpret_cast<const char*>(Data.data()), Data.size()), Data.size()));
        return nullptr;
    }

    if (!TCPSend(*Client, StringToVector("A"))) { // changed to A for Accepted version
        Disconnect(Client);
        return nullptr;
    }

    Data = TCPRcv(*Client);

    if (Data.size() > 50) {
        ClientKick(*Client, "Invalid Key (too long)!");
        return nullptr;
    }

    std::string key(reinterpret_cast<const char*>(Data.data()), Data.size());

    nlohmann::json AuthReq {};
    std::string AuthResStr {};
    try {
        AuthReq = nlohmann::json {
            { "key", key }
        };

        auto Target = "/pkToUser";
        unsigned int ResponseCode = 0;
        AuthResStr = Http::POST(Application::GetBackendUrlForAuth(), 443, Target, AuthReq.dump(), "application/json", &ResponseCode);

    } catch (const std::exception& e) {
        beammp_debugf("Invalid json sent by client, kicking: {}", e.what());
        ClientKick(*Client, "Invalid Key (invalid UTF8 string)!");
        return nullptr;
    }

    try {
        nlohmann::json AuthRes = nlohmann::json::parse(AuthResStr);

        if (AuthRes["username"].is_string() && AuthRes["roles"].is_string()
            && AuthRes["guest"].is_boolean() && AuthRes["identifiers"].is_array()) {

            Client->SetName(AuthRes["username"]);
            Client->SetRoles(AuthRes["roles"]);
            Client->IsGuest = AuthRes["guest"];
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

    if (!Application::Settings.Password.empty()) { // ask password
        if (!TCPSend(*Client, StringToVector("S"))) {
            Disconnect(Client);
            return {};
        }
        beammp_info("Waiting for password");
        Data = TCPRcv(*Client);
        std::string Pass = std::string(reinterpret_cast<const char*>(Data.data()), Data.size());
        if (Pass != HashPassword(Application::Settings.Password)) {
            beammp_debug(Client->Name.get() + " attempted to connect with a wrong password");
            ClientKick(*Client, "Wrong password!");
            return {};
        } else {
            beammp_debug(Client->Name.get() + " used the correct password");
        }
    }

    beammp_debug("Name-> " + Client->Name.get() + ", Guest-> " + std::to_string(Client->IsGuest.get()) + ", Roles-> " + Client->Role.get());
    mServer.ForEachClient([&](const std::shared_ptr<TClient>& Cl) -> bool {
        if (Cl->Name.get() == Client->Name.get() && Cl->IsGuest == Client->IsGuest) {
            Disconnect(Cl);
            return false;
        }
        return true;
    });

    auto Futures = LuaAPI::MP::Engine->TriggerEvent("onPlayerAuth", "", Client->Name.get(), Client->Role.get(), Client->IsGuest.get(), Client->Identifiers.get());
    TLuaEngine::WaitForAll(Futures);
    bool NotAllowed = std::any_of(Futures.begin(), Futures.end(),
        [](const std::shared_ptr<TLuaResult>& Result) {
            return !Result->Error && Result->Result.is<int>() && bool(Result->Result.as<int>());
        });
    std::string Reason;
    bool NotAllowedWithReason = std::any_of(Futures.begin(), Futures.end(),
        [&Reason](const std::shared_ptr<TLuaResult>& Result) -> bool {
            if (!Result->Error && Result->Result.is<std::string>()) {
                Reason = Result->Result.as<std::string>();
                return true;
            }
            return false;
        });

    if (NotAllowed) {
        ClientKick(*Client, "you are not allowed on the server!");
        return {};
    } else if (NotAllowedWithReason) {
        ClientKick(*Client, Reason);
        return {};
    }

    if (mServer.ClientCount() < size_t(Application::Settings.MaxPlayers)) {
        beammp_info("Identification success");
        mServer.InsertClient(Client);
        try {
            TCPClient(Client);
        } catch (const std::exception& e) {
            beammp_infof("Client {} disconnected: {}", Client->ID.get(), e.what());
            Disconnect(Client);
            return {};
        }
    } else {
        ClientKick(*Client, "Server full!");
    }

    return Client;
}

std::shared_ptr<TClient> TNetwork::CreateClient(ip::tcp::socket&& TCPSock) {
    auto c = std::make_shared<TClient>(mServer, std::move(TCPSock));
    return c;
}

bool TNetwork::TCPSend(TClient& c, const std::vector<uint8_t>& Data, bool IsSync) {
    if (!IsSync) {
        if (c.IsSyncing) {
            if (!Data.empty()) {
                if (Data.at(0) == 'O' || Data.at(0) == 'A' || Data.at(0) == 'C' || Data.at(0) == 'E') {
                    c.EnqueuePacket(Data);
                }
            }
            return true;
        }
    }

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
    write(*c.TCPSocket.synchronize(), buffer(ToSend), ec);
    if (ec) {
        beammp_debugf("write(): {}", ec.message());
        Disconnect(c);
        return false;
    }
    c.UpdatePingTime();
    return true;
}

std::vector<uint8_t> TNetwork::TCPRcv(TClient& c) {
    if (c.IsDisconnected()) {
        beammp_error("Client disconnected, cancelling TCPRcv");
        return {};
    }

    int32_t Header {};

    boost::system::error_code ec;
    std::array<uint8_t, sizeof(Header)> HeaderData;
    read(*c.TCPSocket.synchronize(), buffer(HeaderData), ec);
    if (ec) {
        // TODO: handle this case (read failed)
        beammp_debugf("TCPRcv: Reading header failed: {}", ec.message());
        return {};
    }
    Header = *reinterpret_cast<int32_t*>(HeaderData.data());

    if (Header < 0) {
        ClientKick(c, "Invalid packet->header negative");
        beammp_errorf("Client {} send negative TCP header, ignoring packet", c.ID.get());
        return {};
    }

    std::vector<uint8_t> Data;
    // TODO: This is arbitrary, this needs to be handled another way
    if (Header < int32_t(100 * MB)) {
        Data.resize(Header);
    } else {
        ClientKick(c, "Header size limit exceeded");
        beammp_warn("Client " + c.Name.get() + " (" + std::to_string(c.ID.get()) + ") sent header of >100MB->assuming malicious intent and disconnecting the client.");
        return {};
    }
    auto N = read(*c.TCPSocket.synchronize(), buffer(Data), ec);
    if (ec) {
        // TODO: handle this case properly
        beammp_debugf("TCPRcv: Reading data failed: {}", ec.message());
        return {};
    }

    if (N != Header) {
        beammp_errorf("Expected to read {} bytes, instead got {}", Header, N);
    }

    constexpr std::string_view ABG = "ABG:";
    if (Data.size() >= ABG.size() && std::equal(Data.begin(), Data.begin() + ABG.size(), ABG.begin(), ABG.end())) {
        Data.erase(Data.begin(), Data.begin() + ABG.size());
        return DeComp(Data);
    } else {
        return Data;
    }
}

void TNetwork::ClientKick(TClient& c, const std::string& R) {
    beammp_info("Client kicked: " + R);
    if (!TCPSend(c, StringToVector("K" + R))) {
        beammp_debugf("tried to kick player '{}' (id {}), but was already disconnected", c.Name.get(), c.ID.get());
    }
    Disconnect(c);
}

void TNetwork::Looper(const std::shared_ptr<TClient>& Client) {
    RegisterThreadAuto();
    while (!Client->IsDisconnected()) {
        if (!Client->IsSyncing.get() && Client->IsSynced.get() && Client->MissedPacketsQueue->size() != 0) {
            while (Client->MissedPacketsQueue->size() > 0) {
                std::vector<uint8_t> QData {};
                { // locked context
                    auto Lock = Client->MissedPacketsQueue;
                    if (Lock->size() <= 0) {
                        break;
                    }
                    QData = Lock->front();
                    Lock->pop();
                } // end locked context
                if (!TCPSend(*Client, QData, true)) {
                    Disconnect(Client);
                    auto Lock = Client->MissedPacketsQueue;
                    while (!Lock->empty()) {
                        Lock->pop();
                    }
                    break;
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void TNetwork::TCPClient(const std::shared_ptr<TClient>& c) {
    OnConnect(c);
    RegisterThread("(" + std::to_string(c->ID.get()) + ") \"" + c->Name.get() + "\"");

    std::thread QueueSync(&TNetwork::Looper, this, c);

    while (true) {
        if (c->IsDisconnected()) {
            beammp_debug("client status < 0, breaking client loop");
            break;
        }

        auto res = TCPRcv(*c);
        if (res.empty()) {
            beammp_debug("TCPRcv empty");
            Disconnect(c);
            break;
        }
        mServer.GlobalParser(c, std::move(res), mPPSMonitor, *this);
    }

    if (QueueSync.joinable())
        QueueSync.join();

    Disconnect(c);
}

void TNetwork::UpdatePlayer(TClient& Client) {
    std::string Packet = ("Ss") + std::to_string(mServer.ClientCount()) + "/" + std::to_string(Application::Settings.MaxPlayers) + ":";
    mServer.ForEachClient([&](const std::shared_ptr<TClient>& Client) -> bool {
        ReadLock Lock(mServer.GetClientMutex());
        Packet += Client->Name.get() + ",";
        return true;
    });
    Packet = Packet.substr(0, Packet.length() - 1);
    Client.EnqueuePacket(StringToVector(Packet));
    //(void)Respond(Client, Packet, true);
}

void TNetwork::Disconnect(const std::weak_ptr<TClient>& ClientPtr) {
    // this is how one checks that the ClientPtr is not empty (as opposed to expired)
    if (ClientPtr.owner_before(std::weak_ptr<TClient> {})) {
        return;
    }
    std::shared_ptr<TClient> LockedClientPtr { nullptr };
    try {
        LockedClientPtr = ClientPtr.lock();
    } catch (const std::exception&) {
        beammp_warn("Client expired in CloseSockets, this is unexpected");
        return;
    }
    beammp_assert(LockedClientPtr != nullptr);
    TClient& c = *LockedClientPtr;
    Disconnect(c);
}
void TNetwork::Disconnect(TClient& Client) {
    beammp_info(Client.Name.get() + (" Connection Terminated"));
    std::string Packet;
    {
        auto Locked = Client.VehicleData.synchronize();
        for (auto& v : *Locked) {
            LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onVehicleDeleted", "", Client.ID.get(), v.ID()));
            Packet = "Od:" + std::to_string(Client.ID.get()) + "-" + std::to_string(v.ID());
            SendToAll(&Client, StringToVector(Packet), false, true);
        }
    }
    Packet = ("L") + Client.Name.get() + (" left the server!");
    SendToAll(&Client, StringToVector(Packet), false, true);
    Packet.clear();
    auto Futures = LuaAPI::MP::Engine->TriggerEvent("onPlayerDisconnect", "", Client.ID.get());
    TLuaEngine::WaitForAll(Futures);
    Client.CloseSockets("Normal disconnect");
    mServer.RemoveClient(Client);
}
void TNetwork::Disconnect(const std::shared_ptr<TClient>& ClientPtr) {
    if (ClientPtr == nullptr) {
        return;
    }
    Disconnect(*ClientPtr);
}

void TNetwork::OnConnect(const std::weak_ptr<TClient>& c) {
    beammp_assert(!c.expired());
    beammp_info("Client connected");
    auto LockedClient = c.lock();
    mServer.ClaimFreeIDFor(*LockedClient);
    beammp_info("Assigned ID " + std::to_string(LockedClient->ID.get()) + " to " + LockedClient->Name.get());
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerConnecting", "", LockedClient->ID.get()));
    SyncResources(*LockedClient);
    if (LockedClient->IsDisconnected())
        return;
    (void)Respond(*LockedClient, StringToVector("M" + Application::Settings.MapName), true); // Send the Map on connect
    beammp_info(LockedClient->Name.get() + " : Connected");
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerJoining", "", LockedClient->ID.get()));
}

void TNetwork::SyncResources(TClient& c) {
    if (!TCPSend(c, StringToVector("P" + std::to_string(c.ID.get())))) {
        throw std::runtime_error("Failed to send 'P' to client");
    }
    std::vector<uint8_t> Data;
    while (!c.IsDisconnected()) {
        Data = TCPRcv(c);
        if (Data.empty()) {
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
            std::string ToSend = mResourceManager.FileList() + mResourceManager.FileSizes();
            if (ToSend.empty())
                ToSend = "-";
            if (!TCPSend(c, StringToVector(ToSend))) {
                throw std::runtime_error("Failed to send packet to client");
            }
        }
        return;
    default:
        return;
    }
}

void TNetwork::SendFile(TClient& c, const std::string& UnsafeName) {
    beammp_info(c.Name.get() + " requesting : " + UnsafeName.substr(UnsafeName.find_last_of('/')));

    if (!fs::path(UnsafeName).has_filename()) {
        if (!TCPSend(c, StringToVector("CO"))) {
            Disconnect(c);
            return;
        }
        beammp_warn("File " + UnsafeName + " is not a file!");
        return;
    }
    auto FileName = fs::path(UnsafeName).filename().string();
    FileName = Application::Settings.Resource + "/Client/" + FileName;

    if (!std::filesystem::exists(FileName)) {
        if (!TCPSend(c, StringToVector("CO"))) {
            Disconnect(c);
            return;
        }
        beammp_warn("File " + UnsafeName + " could not be accessed!");
        return;
    }

    if (!TCPSend(c, StringToVector("AG"))) {
        Disconnect(c);
        return;
    }

    /// Wait for connections
    int T = 0;
    while (!c.DownSocket->is_open() && T < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        T++;
    }

    if (!c.DownSocket->is_open()) {
        beammp_error("Client doesn't have a download socket!");
        if (!c.IsDisconnected())
            Disconnect(c);
        return;
    }

    size_t Size = size_t(std::filesystem::file_size(FileName)), MSize = Size / 2;

    std::thread SplitThreads[2] {
        std::thread([&] {
            RegisterThread("SplitLoad_0");
            SplitLoad(c, 0, MSize, false, FileName);
        }),
        std::thread([&] {
            RegisterThread("SplitLoad_1");
            SplitLoad(c, MSize, Size, true, FileName);
        })
    };

    for (auto& SplitThread : SplitThreads) {
        if (SplitThread.joinable()) {
            SplitThread.join();
        }
    }
}

static std::pair<size_t /* count */, size_t /* last chunk */> SplitIntoChunks(size_t FullSize, size_t ChunkSize) {
    if (FullSize < ChunkSize) {
        return { 0, FullSize };
    }
    size_t Count = FullSize / (FullSize / ChunkSize);
    size_t LastChunkSize = FullSize - (Count * ChunkSize);
    return { Count, LastChunkSize };
}

TEST_CASE("SplitIntoChunks") {
    size_t FullSize;
    size_t ChunkSize;
    SUBCASE("Normal case") {
        FullSize = 1234567;
        ChunkSize = 1234;
    }
    SUBCASE("Zero original size") {
        FullSize = 0;
        ChunkSize = 100;
    }
    SUBCASE("Equal full size and chunk size") {
        FullSize = 125;
        ChunkSize = 125;
    }
    SUBCASE("Even split") {
        FullSize = 10000;
        ChunkSize = 100;
    }
    SUBCASE("Odd split") {
        FullSize = 13;
        ChunkSize = 2;
    }
    SUBCASE("Large sizes") {
        FullSize = 10 * GB;
        ChunkSize = 125 * MB;
    }
    auto [Count, LastSize] = SplitIntoChunks(FullSize, ChunkSize);
    CHECK((Count * ChunkSize) + LastSize == FullSize);
}

const uint8_t* /* end ptr */ TNetwork::SendSplit(TClient& c, ip::tcp::socket& Socket, const uint8_t* DataPtr, size_t Size) {
    if (TCPSendRaw(c, Socket, DataPtr, Size)) {
        return DataPtr + Size;
    } else {
        return nullptr;
    }
}

void TNetwork::SplitLoad(TClient& c, size_t Sent, size_t Size, bool D, const std::string& Name) {
    std::ifstream f(Name.c_str(), std::ios::binary);
    uint32_t Split = 125 * MB;
    std::vector<uint8_t> Data;
    if (Size > Split)
        Data.resize(Split);
    else
        Data.resize(Size);

    if (D) {
        auto TCPSock = c.DownSocket.synchronize();
        while (!c.IsDisconnected() && Sent < Size) {
            size_t Diff = Size - Sent;
            if (Diff > Split) {
                f.seekg(Sent, std::ios_base::beg);
                f.read(reinterpret_cast<char*>(Data.data()), Split);
                if (!TCPSendRaw(c, *TCPSock, Data.data(), Split)) {
                    if (!c.IsDisconnected())
                        Disconnect(c);
                    break;
                }
                Sent += Split;
            } else {
                f.seekg(Sent, std::ios_base::beg);
                f.read(reinterpret_cast<char*>(Data.data()), Diff);
                if (!TCPSendRaw(c, *TCPSock, Data.data(), int32_t(Diff))) {
                    if (!c.IsDisconnected())
                        Disconnect(c);
                    break;
                }
                Sent += Diff;
            }
        }
    } else {
        auto TCPSock = c.TCPSocket.synchronize();
        while (!c.IsDisconnected() && Sent < Size) {
            size_t Diff = Size - Sent;
            if (Diff > Split) {
                f.seekg(Sent, std::ios_base::beg);
                f.read(reinterpret_cast<char*>(Data.data()), Split);
                if (!TCPSendRaw(c, *TCPSock, Data.data(), Split)) {
                    if (!c.IsDisconnected())
                        Disconnect(c);
                    break;
                }
                Sent += Split;
            } else {
                f.seekg(Sent, std::ios_base::beg);
                f.read(reinterpret_cast<char*>(Data.data()), Diff);
                if (!TCPSendRaw(c, *TCPSock, Data.data(), int32_t(Diff))) {
                    if (!c.IsDisconnected())
                        Disconnect(c);
                    break;
                }
                Sent += Diff;
            }
        }
    }
}

bool TNetwork::TCPSendRaw(TClient& C, ip::tcp::socket& socket, const uint8_t* Data, size_t Size) {
    boost::system::error_code ec;
    write(socket, buffer(Data, Size), ec);
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
    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
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
    if (c.expired()) {
        return false;
    }
    auto LockedClient = c.lock();
    if (LockedClient->IsSynced.get())
        return true;
    // Syncing, later set isSynced
    // after syncing is done, we apply all packets they missed
    if (!Respond(*LockedClient, StringToVector("Sn" + LockedClient->Name.get()), true)) {
        return false;
    }
    // ignore error
    (void)SendToAll(LockedClient.get(), StringToVector("JWelcome " + LockedClient->Name.get() + "!"), false, true);

    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerJoin", "", LockedClient->ID.get()));
    LockedClient->IsSyncing = true;
    bool Return = false;
    bool res = true;
    mServer.ForEachClient([&](const std::shared_ptr<TClient>& Client) -> bool {
        auto VehicleData = Client->VehicleData.synchronize();
        if (Client != LockedClient) {
            for (auto& v : *VehicleData) {
                if (LockedClient->IsDisconnected()) {
                    Return = true;
                    res = false;
                    return false;
                }
                res = Respond(*LockedClient, StringToVector(v.Data()), true, true);
            }
        }

        return true;
    });
    LockedClient->IsSyncing = false;
    if (Return) {
        return res;
    }
    LockedClient->IsSynced = true;
    beammp_info(LockedClient->Name.get() + (" is now synced!"));
    return true;
}

void TNetwork::SendToAll(TClient* c, const std::vector<uint8_t>& Data, bool Self, bool Rel) {
    if (!Self)
        beammp_assert(c);
    char C = static_cast<char>(Data.at(0));
    mServer.ForEachClient([&](const std::shared_ptr<TClient>& Client) -> bool {
        if (Self || Client.get() != c) {
            if (Client->IsSynced || Client->IsSyncing) {
                if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
                    if (C == 'O' || C == 'T' || Data.size() > 1000) {
                        if (Data.size() > 400) {
                            auto CompressedData = Data;
                            CompressProperly(CompressedData);
                            Client->EnqueuePacket(CompressedData);
                        } else {
                            Client->EnqueuePacket(Data);
                        }
                    } else {
                        Client->EnqueuePacket(Data);
                    }
                } else {
                    if (!UDPSend(*Client, Data)) {
                        Disconnect(Client);
                    }
                }
            }
        }
        return true;
    });
}

bool TNetwork::UDPSend(TClient& Client, std::vector<uint8_t> Data) {
    if (!Client.IsConnected || Client.IsDisconnected()) {
        // this can happen if we try to send a packet to a client that is either
        // 1. not yet fully connected, or
        // 2. disconnected and not yet fully removed
        // this is fine can can be ignored :^)
        return true;
    }
    const auto Addr = Client.UDPAddress;
    if (Data.size() > 400) {
        CompressProperly(Data);
    }
    boost::system::error_code ec;
    mUDPSock.send_to(buffer(Data), *Addr, 0, ec);
    if (ec) {
        beammp_debugf("UDP sendto() failed: {}", ec.message());
        if (!Client.IsDisconnected())
            Disconnect(Client);
        return false;
    }
    return true;
}

std::vector<uint8_t> TNetwork::UDPRcvFromClient(ip::udp::endpoint& ClientEndpoint) {
    std::array<char, 1024> Ret {};
    boost::system::error_code ec;
    const auto Rcv = mUDPSock.receive_from(mutable_buffer(Ret.data(), Ret.size()), ClientEndpoint, 0, ec);
    if (ec) {
        beammp_errorf("UDP recvfrom() failed: {}", ec.message());
        return {};
    }
    beammp_assert(Rcv <= Ret.size());
    return std::vector<uint8_t>(Ret.begin(), Ret.begin() + Rcv);
}
