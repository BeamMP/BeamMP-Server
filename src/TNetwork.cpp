#include "TNetwork.h"
#include "Client.h"
#include "LuaAPI.h"
#include "TLuaEngine.h"
#include <CustomAssert.h>
#include <Http.h>
#include <array>
#include <cstring>

TNetwork::TNetwork(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager)
    : mServer(Server)
    , mPPSMonitor(PPSMonitor)
    , mResourceManager(ResourceManager) {
    Application::SetSubsystemStatus("TCPNetwork", Application::Status::Starting);
    Application::SetSubsystemStatus("UDPNetwork", Application::Status::Starting);
    Application::RegisterShutdownHandler([&] {
        beammp_debug("Kicking all players due to shutdown");
        Server.ForEachClient([&](std::weak_ptr<TClient> client) -> bool {
            if (!client.expired()) {
                ClientKick(*client.lock(), "Server shutdown");
            }
            return true;
        });
    });
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("UDPNetwork", Application::Status::ShuttingDown);
        if (mUDPThread.joinable()) {
            mShutdown = true;
            mUDPThread.detach();
        }
        Application::SetSubsystemStatus("UDPNetwork", Application::Status::Shutdown);
    });
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("TCPNetwork", Application::Status::ShuttingDown);
        if (mTCPThread.joinable()) {
            mShutdown = true;
            mTCPThread.detach();
        }
        Application::SetSubsystemStatus("TCPNetwork", Application::Status::Shutdown);
    });
    mTCPThread = std::thread(&TNetwork::TCPServerMain, this);
    mUDPThread = std::thread(&TNetwork::UDPServerMain, this);
}

void TNetwork::UDPServerMain() {
    RegisterThread("UDPServer");
#if defined(BEAMMP_WINDOWS)
    WSADATA data;
    if (WSAStartup(514, &data)) {
        beammp_error(("Can't start Winsock!"));
        // return;
    }
#endif // WINDOWS
    mUDPSock = socket(AF_INET, SOCK_DGRAM, 0);
    // Create a server hint structure for the server
    sockaddr_in serverAddr {};
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Any Local
    serverAddr.sin_family = AF_INET; // Address format is IPv4
    serverAddr.sin_port = htons(uint16_t(Application::Settings.Port)); // Convert from little to big endian

    // Try and bind the socket to the IP and port
    if (bind(mUDPSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        beammp_error("bind() failed: " + GetPlatformAgnosticErrorString());
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1); // TODO: Wtf.
        // return;
    }
    Application::SetSubsystemStatus("UDPNetwork", Application::Status::Good);
    beammp_info(("Vehicle data network online on port ") + std::to_string(Application::Settings.Port) + (" with a Max of ")
        + std::to_string(Application::Settings.MaxPlayers) + (" Clients"));
    while (!mShutdown) {
        try {
            sockaddr_in client {};
            std::string Data = UDPRcvFromClient(client); // Receives any data from Socket
            size_t Pos = Data.find(':');
            if (Data.empty() || Pos > 2)
                continue;
            /*char clientIp[256];
            ZeroMemory(clientIp, 256); ///Code to get IP we don't need that yet
            inet_ntop(AF_INET, &client.sin_addr, clientIp, 256);*/
            uint8_t ID = uint8_t(Data.at(0)) - 1;
            mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
                std::shared_ptr<TClient> Client;
                {
                    ReadLock Lock(mServer.GetClientMutex());
                    if (!ClientPtr.expired()) {
                        Client = ClientPtr.lock();
                    } else
                        return true;
                }

                if (Client->GetID() == ID) {
                    Client->SetUDPAddr(client);
                    Client->SetIsConnected(true);
                    TServer::GlobalParser(ClientPtr, Data.substr(2), mPPSMonitor, *this);
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
#if defined(BEAMMP_WINDOWS)
    WSADATA wsaData;
    if (WSAStartup(514, &wsaData)) {
        beammp_error("Can't start Winsock!");
        return;
    }
#endif // WINDOWS
    TConnection client {};
    SOCKET Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int optval = 1;
#if defined(BEAMMP_WINDOWS)
    const char* optval_ptr = reinterpret_cast<const char*>(&optval);
#elif defined(BEAMMP_LINUX) || defined(BEAMMP_APPLE)
    void* optval_ptr = reinterpret_cast<void*>(&optval);
#endif
    setsockopt(Listener, SOL_SOCKET, SO_REUSEADDR, optval_ptr, sizeof(optval));
    // TODO: check optval or return value idk
    sockaddr_in addr {};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(Application::Settings.Port));
    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) != 0) {
        beammp_error("bind() failed: " + GetPlatformAgnosticErrorString());
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1); // TODO: Wtf.
    }
    if (Listener == -1) {
        beammp_error("Invalid listening socket");
        return;
    }
    if (listen(Listener, SOMAXCONN)) {
        beammp_error("listen() failed: " + GetPlatformAgnosticErrorString());
        // FIXME leak Listener
        return;
    }
    Application::SetSubsystemStatus("TCPNetwork", Application::Status::Good);
    beammp_info(("Vehicle event network online"));
    do {
        try {
            if (mShutdown) {
                beammp_debug("shutdown during TCP wait for accept loop");
                break;
            }
            client.SockAddrLen = sizeof(client.SockAddr);
            client.Socket = accept(Listener, &client.SockAddr, &client.SockAddrLen);
            if (client.Socket == -1) {
                beammp_warn(("Got an invalid client socket on connect! Skipping..."));
                continue;
            }
            // set timeout (DWORD, aka uint32_t)
            uint32_t SendTimeoutMS = 30 * 1000;
#if defined(BEAMMP_WINDOWS)
            int ret = ::setsockopt(client.Socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&SendTimeoutMS), sizeof(SendTimeoutMS));
#else // POSIX
            struct timeval optval;
            optval.tv_sec = (int)(SendTimeoutMS / 1000);
            optval.tv_usec = (SendTimeoutMS % 1000) * 1000;
            int ret = ::setsockopt(client.Socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<void*>(&optval), sizeof(optval));
#endif
            if (ret < 0) {
                throw std::runtime_error("setsockopt recv timeout: " + GetPlatformAgnosticErrorString());
            }
            std::thread ID(&TNetwork::Identify, this, client);
            ID.detach(); // TODO: Add to a queue and attempt to join periodically
        } catch (const std::exception& e) {
            beammp_error(("fatal: ") + std::string(e.what()));
        }
    } while (client.Socket);

    beammp_debug("all ok, arrived at " + std::string(__func__) + ":" + std::to_string(__LINE__));

    CloseSocketProper(client.Socket);
#ifdef BEAMMP_WINDOWS
    CloseSocketProper(client.Socket);
    WSACleanup();
#endif // WINDOWS
}

#undef GetObject // Fixes Windows

#include "Json.h"
namespace json = rapidjson;

void TNetwork::Identify(const TConnection& client) {
    RegisterThreadAuto();
    char Code;
    if (recv(client.Socket, &Code, 1, 0) != 1) {
        CloseSocketProper(client.Socket);
        return;
    }
    if (Code == 'C') {
        Authentication(client);
    } else if (Code == 'D') {
        HandleDownload(client.Socket);
    } else if (Code == 'P') {
#if defined(BEAMMP_LINUX) || defined(BEAMMP_APPLE)
        send(client.Socket, "P", 1, MSG_NOSIGNAL);
#else
        send(client.Socket, "P", 1, 0);
#endif
        CloseSocketProper(client.Socket);
        return;
    } else {
        CloseSocketProper(client.Socket);
    }
}

void TNetwork::HandleDownload(SOCKET TCPSock) {
    char D;
    if (recv(TCPSock, &D, 1, 0) != 1) {
        CloseSocketProper(TCPSock);
        return;
    }
    auto ID = uint8_t(D);
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        ReadLock Lock(mServer.GetClientMutex());
        if (!ClientPtr.expired()) {
            auto c = ClientPtr.lock();
            if (c->GetID() == ID) {
                c->SetDownSock(TCPSock);
            }
        }
        return true;
    });
}

void TNetwork::Authentication(const TConnection& ClientConnection) {
    auto Client = CreateClient(ClientConnection.Socket);
    char AddrBuf[64];
    // TODO: IPv6 would need this to be changed
    auto str = inet_ntop(AF_INET, reinterpret_cast<const void*>(&ClientConnection.SockAddr), AddrBuf, sizeof(ClientConnection.SockAddr));
    beammp_trace("This thread is ip " + std::string(str));
    Client->SetIdentifier("ip", str);

    std::string Rc; // TODO: figure out why this is not default constructed
    beammp_info("Identifying new ClientConnection...");

    Rc = TCPRcv(*Client);

    if (Rc.size() > 3 && Rc.substr(0, 2) == "VC") {
        Rc = Rc.substr(2);
        if (Rc.length() > 4 || Rc != Application::ClientVersionString()) {
            ClientKick(*Client, "Outdated Version!");
            return;
        }
    } else {
        ClientKick(*Client, "Invalid version header!");
        return;
    }
    if (!TCPSend(*Client, "S")) {
        // TODO: handle
    }

    Rc = TCPRcv(*Client);

    if (Rc.size() > 50) {
        ClientKick(*Client, "Invalid Key!");
        return;
    }

    auto RequestString = R"({"key":")" + Rc + "\"}";

    auto Target = "/pkToUser";
    unsigned int ResponseCode = 0;
    if (!Rc.empty()) {
        Rc = Http::POST(Application::GetBackendUrlForAuth(), 443, Target, RequestString, "application/json", &ResponseCode);
    }

    json::Document AuthResponse;
    AuthResponse.Parse(Rc.c_str());
    if (Rc == Http::ErrorString || AuthResponse.HasParseError()) {
        ClientKick(*Client, "Invalid key! Please restart your game.");
        return;
    }

    if (!AuthResponse.IsObject()) {
        if (Rc == "0") {
            auto Lock = Sentry.CreateExclusiveContext();
            Sentry.SetContext("auth",
                { { "response-body", Rc },
                    { "key", RequestString } });
            Sentry.SetTransaction(Application::GetBackendUrlForAuth() + Target);
            Sentry.Log(SentryLevel::Info, "default", "backend returned 0 instead of json (" + std::to_string(ResponseCode) + ")");
        } else { // Rc != "0"
            ClientKick(*Client, "Backend returned invalid auth response format.");
            beammp_error("Backend returned invalid auth response format. This should never happen.");
            auto Lock = Sentry.CreateExclusiveContext();
            Sentry.SetContext("auth",
                { { "response-body", Rc },
                    { "key", RequestString } });
            Sentry.SetTransaction(Application::GetBackendUrlForAuth() + Target);
            Sentry.Log(SentryLevel::Error, "default", "unexpected backend response (" + std::to_string(ResponseCode) + ")");
        }
        return;
    }

    if (AuthResponse["username"].IsString() && AuthResponse["roles"].IsString()
        && AuthResponse["guest"].IsBool() && AuthResponse["identifiers"].IsArray()) {

        Client->SetName(AuthResponse["username"].GetString());
        Client->SetRoles(AuthResponse["roles"].GetString());
        Client->SetIsGuest(AuthResponse["guest"].GetBool());
        for (const auto& ID : AuthResponse["identifiers"].GetArray()) {
            auto Raw = std::string(ID.GetString());
            auto SepIndex = Raw.find(':');
            Client->SetIdentifier(Raw.substr(0, SepIndex), Raw.substr(SepIndex + 1));
        }
    } else {
        ClientKick(*Client, "Invalid authentication data!");
        return;
    }

    beammp_debug("Name -> " + Client->GetName() + ", Guest -> " + std::to_string(Client->IsGuest()) + ", Roles -> " + Client->GetRoles());
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        std::shared_ptr<TClient> Cl;
        {
            ReadLock Lock(mServer.GetClientMutex());
            if (!ClientPtr.expired()) {
                Cl = ClientPtr.lock();
            } else
                return true;
        }
        if (Cl->GetName() == Client->GetName() && Cl->IsGuest() == Client->IsGuest()) {
            CloseSocketProper(Cl->GetTCPSock());
            Cl->SetStatus(-2);
            return false;
        }

        return true;
    });

    auto Futures = LuaAPI::MP::Engine->TriggerEvent("onPlayerAuth", "", Client->GetName(), Client->GetRoles(), Client->IsGuest());
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
        return;
    } else if (NotAllowedWithReason) {
        ClientKick(*Client, Reason);
        return;
    }

    if (mServer.ClientCount() < size_t(Application::Settings.MaxPlayers)) {
        beammp_info("Identification success");
        mServer.InsertClient(Client);
        TCPClient(Client);
    } else
        ClientKick(*Client, "Server full!");
}

std::shared_ptr<TClient> TNetwork::CreateClient(SOCKET TCPSock) {
    auto c = std::make_shared<TClient>(mServer);
    c->SetTCPSock(TCPSock);
    return c;
}

bool TNetwork::TCPSend(TClient& c, const std::string& Data, bool IsSync) {
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

    int32_t Size, Sent;
    std::string Send(4, 0);
    Size = int32_t(Data.size());
    memcpy(&Send[0], &Size, sizeof(Size));
    Send += Data;
    Sent = 0;
    Size += 4;
    do {
#if defined(BEAMMP_WINDOWS)
        int32_t Temp = send(c.GetTCPSock(), &Send[Sent], Size - Sent, 0);
#elif defined(BEAMMP_LINUX) || defined(BEAMMP_APPLE)
        int32_t Temp = send(c.GetTCPSock(), &Send[Sent], Size - Sent, MSG_NOSIGNAL);
#endif
        if (Temp == 0) {
            beammp_debug("send() == 0: " + GetPlatformAgnosticErrorString());
            if (c.GetStatus() > -1)
                c.SetStatus(-1);
            return false;
        } else if (Temp < 0) {
            beammp_debug("send() < 0: " + GetPlatformAgnosticErrorString()); // TODO fix it was spamming yet everyone stayed on the server
            if (c.GetStatus() > -1)
                c.SetStatus(-1);
            CloseSocketProper(c.GetTCPSock());
            return false;
        }
        Sent += Temp;
        c.UpdatePingTime();
    } while (Sent < Size);
    return true;
}

bool TNetwork::CheckBytes(TClient& c, int32_t BytesRcv) {
    if (BytesRcv == 0) {
        beammp_trace("(TCP) Connection closing...");
        if (c.GetStatus() > -1)
            c.SetStatus(-1);
        return false;
    } else if (BytesRcv < 0) {
        beammp_debug("(TCP) recv() failed: " + GetPlatformAgnosticErrorString());
        if (c.GetStatus() > -1)
            c.SetStatus(-1);
        CloseSocketProper(c.GetTCPSock());
        return false;
    }
    return true;
}

std::string TNetwork::TCPRcv(TClient& c) {
    int32_t Header, BytesRcv = 0, Temp;
    if (c.GetStatus() < 0)
        return "";

    std::vector<char> Data(sizeof(Header));
    do {
        Temp = recv(c.GetTCPSock(), &Data[BytesRcv], 4 - BytesRcv, 0);
        if (!CheckBytes(c, Temp)) {
            return "";
        }
        BytesRcv += Temp;
    } while (size_t(BytesRcv) < sizeof(Header));
    memcpy(&Header, &Data[0], sizeof(Header));

    if (!CheckBytes(c, BytesRcv)) {
        return "";
    }
    if (Header < 100 * MB) {
        Data.resize(Header);
    } else {
        ClientKick(c, "Header size limit exceeded");
        beammp_warn("Client " + c.GetName() + " (" + std::to_string(c.GetID()) + ") sent header of >100MB - assuming malicious intent and disconnecting the client.");
        return "";
    }
    BytesRcv = 0;
    do {
        Temp = recv(c.GetTCPSock(), &Data[BytesRcv], Header - BytesRcv, 0);
        if (!CheckBytes(c, Temp)) {
            return "";
        }
        BytesRcv += Temp;
    } while (BytesRcv < Header);
    std::string Ret(Data.data(), Header);

    if (Ret.substr(0, 4) == "ABG:") {
        Ret = DeComp(Ret.substr(4));
    }
    return Ret;
}

void TNetwork::ClientKick(TClient& c, const std::string& R) {
    beammp_info("Client kicked: " + R);
    if (!TCPSend(c, "K" + R)) {
        // TODO handle
    }
    c.SetStatus(-2);

    if (c.GetTCPSock())
        CloseSocketProper(c.GetTCPSock());

    if (c.GetDownSock())
        CloseSocketProper(c.GetDownSock());
}

void TNetwork::Looper(const std::weak_ptr<TClient>& c) {
    RegisterThreadAuto();
    while (!c.expired()) {
        auto Client = c.lock();
        if (Client->GetStatus() < 0) {
            beammp_debug("client status < 0, breaking client loop");
            break;
        }
        if (!Client->IsSyncing() && Client->IsSynced() && Client->MissedPacketQueueSize() != 0) {
            // debug("sending " + std::to_string(Client->MissedPacketQueueSize()) + " queued packets");
            while (Client->MissedPacketQueueSize() > 0) {
                std::string QData {};
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
                    if (Client->GetStatus() > -1)
                        Client->SetStatus(-1);
                    {
                        std::unique_lock lock(Client->MissedPacketQueueMutex());
                        while (!Client->MissedPacketQueue().empty()) {
                            Client->MissedPacketQueue().pop();
                        }
                    }
                    CloseSocketProper(Client->GetTCPSock());
                    break;
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void TNetwork::TCPClient(const std::weak_ptr<TClient>& c) {
    // TODO: the c.expired() might cause issues here, remove if you end up here with your debugger
    if (c.expired() || c.lock()->GetTCPSock() == -1) {
        mServer.RemoveClient(c);
        return;
    }
    OnConnect(c);
    RegisterThread("(" + std::to_string(c.lock()->GetID()) + ") \"" + c.lock()->GetName() + "\"");

    std::thread QueueSync(&TNetwork::Looper, this, c);

    while (true) {
        if (c.expired())
            break;
        auto Client = c.lock();
        if (Client->GetStatus() < 0) {
            beammp_debug("client status < 0, breaking client loop");
            break;
        }

        auto res = TCPRcv(*Client);
        if (res == "") {
            beammp_debug("TCPRcv error, break client loop");
            break;
        }
        TServer::GlobalParser(c, res, mPPSMonitor, *this);
    }
    if (QueueSync.joinable())
        QueueSync.join();

    if (!c.expired()) {
        auto Client = c.lock();
        OnDisconnect(c, Client->GetStatus() == -2);
    } else {
        beammp_warn("client expired in TCPClient, should never happen");
    }
}

void TNetwork::UpdatePlayer(TClient& Client) {
    std::string Packet = ("Ss") + std::to_string(mServer.ClientCount()) + "/" + std::to_string(Application::Settings.MaxPlayers) + ":";
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        ReadLock Lock(mServer.GetClientMutex());
        if (!ClientPtr.expired()) {
            auto c = ClientPtr.lock();
            Packet += c->GetName() + ",";
        }
        return true;
    });
    Packet = Packet.substr(0, Packet.length() - 1);
    Client.EnqueuePacket(Packet);
    //(void)Respond(Client, Packet, true);
}

void TNetwork::OnDisconnect(const std::weak_ptr<TClient>& ClientPtr, bool kicked) {
    beammp_assert(!ClientPtr.expired());
    auto LockedClientPtr = ClientPtr.lock();
    TClient& c = *LockedClientPtr;
    beammp_info(c.GetName() + (" Connection Terminated"));
    std::string Packet;
    TClient::TSetOfVehicleData VehicleData;
    { // Vehicle Data Lock Scope
        auto LockedData = c.GetAllCars();
        VehicleData = *LockedData.VehicleData;
    } // End Vehicle Data Lock Scope
    for (auto& v : VehicleData) {
        Packet = "Od:" + std::to_string(c.GetID()) + "-" + std::to_string(v.ID());
        SendToAll(&c, Packet, false, true);
    }
    if (kicked)
        Packet = ("L") + c.GetName() + (" was kicked!");
    else
        Packet = ("L") + c.GetName() + (" left the server!");
    SendToAll(&c, Packet, false, true);
    Packet.clear();
    auto Futures = LuaAPI::MP::Engine->TriggerEvent("onPlayerDisconnect", "", c.GetID());
    LuaAPI::MP::Engine->ReportErrors(Futures);
    if (c.GetTCPSock())
        CloseSocketProper(c.GetTCPSock());
    if (c.GetDownSock())
        CloseSocketProper(c.GetDownSock());
    mServer.RemoveClient(ClientPtr);
}

int TNetwork::OpenID() {
    int ID = 0;
    bool found;
    do {
        found = true;
        mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
            ReadLock Lock(mServer.GetClientMutex());
            if (!ClientPtr.expired()) {
                auto c = ClientPtr.lock();
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
    beammp_assert(!c.expired());
    beammp_info("Client connected");
    auto LockedClient = c.lock();
    LockedClient->SetID(OpenID());
    beammp_info("Assigned ID " + std::to_string(LockedClient->GetID()) + " to " + LockedClient->GetName());
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerConnecting", "", LockedClient->GetID()));
    SyncResources(*LockedClient);
    if (LockedClient->GetStatus() < 0)
        return;
    (void)Respond(*LockedClient, "M" + Application::Settings.MapName, true); // Send the Map on connect
    beammp_info(LockedClient->GetName() + " : Connected");
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerJoining", "", LockedClient->GetID()));
}

void TNetwork::SyncResources(TClient& c) {
#ifndef DEBUG
    try {
#endif
        if (!TCPSend(c, "P" + std::to_string(c.GetID()))) {
            // TODO handle
        }
        std::string Data;
        while (c.GetStatus() > -1) {
            Data = TCPRcv(c);
            if (Data == "Done")
                break;
            Parse(c, Data);
        }
#ifndef DEBUG
    } catch (std::exception& e) {
        beammp_error("Exception! : " + std::string(e.what()));
        c.SetStatus(-1);
    }
#endif
}

void TNetwork::Parse(TClient& c, const std::string& Packet) {
    if (Packet.empty())
        return;
    char Code = Packet.at(0), SubCode = 0;
    if (Packet.length() > 1)
        SubCode = Packet.at(1);
    switch (Code) {
    case 'f':
        SendFile(c, Packet.substr(1));
        return;
    case 'S':
        if (SubCode == 'R') {
            beammp_debug("Sending Mod Info");
            std::string ToSend = mResourceManager.FileList() + mResourceManager.FileSizes();
            if (ToSend.empty())
                ToSend = "-";
            if (!TCPSend(c, ToSend)) {
                // TODO: error
            }
        }
        return;
    default:
        return;
    }
}

void TNetwork::SendFile(TClient& c, const std::string& UnsafeName) {
    beammp_info(c.GetName() + " requesting : " + UnsafeName.substr(UnsafeName.find_last_of('/')));

    if (!fs::path(UnsafeName).has_filename()) {
        if (!TCPSend(c, "CO")) {
            // TODO: handle
        }
        beammp_warn("File " + UnsafeName + " is not a file!");
        return;
    }
    auto FileName = fs::path(UnsafeName).filename().string();
    FileName = Application::Settings.Resource + "/Client/" + FileName;

    if (!std::filesystem::exists(FileName)) {
        if (!TCPSend(c, "CO")) {
            // TODO: handle
        }
        beammp_warn("File " + UnsafeName + " could not be accessed!");
        return;
    }

    if (!TCPSend(c, "AG")) {
        // TODO: handle
    }

    /// Wait for connections
    int T = 0;
    while (c.GetDownSock() < 1 && T < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        T++;
    }

    if (c.GetDownSock() < 1) {
        beammp_error("Client doesn't have a download socket!");
        if (c.GetStatus() > -1)
            c.SetStatus(-1);
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

void TNetwork::SplitLoad(TClient& c, size_t Sent, size_t Size, bool D, const std::string& Name) {
    std::ifstream f(Name.c_str(), std::ios::binary);
    uint32_t Split = 0x7735940; // 125MB
    char* Data;
    if (Size > Split)
        Data = new char[Split];
    else
        Data = new char[Size];
    SOCKET TCPSock;
    if (D)
        TCPSock = c.GetDownSock();
    else
        TCPSock = c.GetTCPSock();
    beammp_debug("Split load Socket " + std::to_string(TCPSock));
    while (c.GetStatus() > -1 && Sent < Size) {
        size_t Diff = Size - Sent;
        if (Diff > Split) {
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Split);
            if (!TCPSendRaw(c, TCPSock, Data, Split)) {
                if (c.GetStatus() > -1)
                    c.SetStatus(-1);
                break;
            }
            Sent += Split;
        } else {
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Diff);
            if (!TCPSendRaw(c, TCPSock, Data, int32_t(Diff))) {
                if (c.GetStatus() > -1)
                    c.SetStatus(-1);
                break;
            }
            Sent += Diff;
        }
    }
    delete[] Data;
    f.close();
}

bool TNetwork::TCPSendRaw(TClient& C, SOCKET socket, char* Data, int32_t Size) {
    intmax_t Sent = 0;
    do {
#if defined(BEAMMP_LINUX) || defined(BEAMMP_APPLE)
        intmax_t Temp = send(socket, &Data[Sent], int(Size - Sent), MSG_NOSIGNAL);
#else
        intmax_t Temp = send(socket, &Data[Sent], int(Size - Sent), 0);
#endif
        if (Temp < 1) {
            beammp_info("Socket Closed! " + std::to_string(socket));
            CloseSocketProper(socket);
            return false;
        }
        Sent += Temp;
        C.UpdatePingTime();
    } while (Sent < Size);
    return true;
}

bool TNetwork::SendLarge(TClient& c, std::string Data, bool isSync) {
    if (Data.length() > 400) {
        std::string CMP(Comp(Data));
        Data = "ABG:" + CMP;
    }
    return TCPSend(c, Data, isSync);
}

bool TNetwork::Respond(TClient& c, const std::string& MSG, bool Rel, bool isSync) {
    char C = MSG.at(0);
    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
        if (C == 'O' || C == 'T' || MSG.length() > 1000) {
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
    if (LockedClient->IsSynced())
        return true;
    // Syncing, later set isSynced
    // after syncing is done, we apply all packets they missed
    if (!Respond(*LockedClient, ("Sn") + LockedClient->GetName(), true)) {
        return false;
    }
    // ignore error
    (void)SendToAll(LockedClient.get(), ("JWelcome ") + LockedClient->GetName() + "!", false, true);

    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onPlayerJoin", "", LockedClient->GetID()));
    LockedClient->SetIsSyncing(true);
    bool Return = false;
    bool res = true;
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        std::shared_ptr<TClient> client;
        {
            ReadLock Lock(mServer.GetClientMutex());
            if (!ClientPtr.expired()) {
                client = ClientPtr.lock();
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
                if (LockedClient->GetStatus() < 0) {
                    Return = true;
                    res = false;
                    return false;
                }
                res = Respond(*LockedClient, v.Data(), true, true);
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

void TNetwork::SendToAll(TClient* c, const std::string& Data, bool Self, bool Rel) {
    if (!Self)
        beammp_assert(c);
    char C = Data.at(0);
    bool ret = true;
    mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
        std::shared_ptr<TClient> Client;
        {
            ReadLock Lock(mServer.GetClientMutex());
            if (!ClientPtr.expired()) {
                Client = ClientPtr.lock();
            } else
                return true;
        }
        if (Self || Client.get() != c) {
            if (Client->IsSynced() || Client->IsSyncing()) {
                if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
                    if (C == 'O' || C == 'T' || Data.length() > 1000) {
                        if (Data.length() > 400) {
                            std::string CMP(Comp(Data));
                            Client->EnqueuePacket("ABG:" + CMP);
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

bool TNetwork::UDPSend(TClient& Client, std::string Data) const {
    if (!Client.IsConnected() || Client.GetStatus() < 0) {
        // this can happen if we try to send a packet to a client that is either
        // 1. not yet fully connected, or
        // 2. disconnected and not yet fully removed
        // this is fine can can be ignored :^)
        return true;
    }
    sockaddr_in Addr = Client.GetUDPAddr();
    auto AddrSize = sizeof(Client.GetUDPAddr());
    if (Data.length() > 400) {
        std::string CMP(Comp(Data));
        Data = "ABG:" + CMP;
    }
#ifdef WIN32
    int sendOk;
    int len = static_cast<int>(Data.size());
#else
    int64_t sendOk;
    size_t len = Data.size();
#endif // WIN32

    sendOk = sendto(mUDPSock, Data.c_str(), len, 0, (sockaddr*)&Addr, int(AddrSize));
    if (sendOk == -1) {
        beammp_debug("(UDP) sendto() failed: " + GetPlatformAgnosticErrorString());
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
        return false;
    } else if (sendOk == 0) {
        beammp_debug(("(UDP) sendto() returned 0"));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
        return false;
    }
    return true;
}

std::string TNetwork::UDPRcvFromClient(sockaddr_in& client) const {
    size_t clientLength = sizeof(client);
    std::array<char, 1024> Ret {};
#ifdef WIN32
    auto Rcv = recvfrom(mUDPSock, Ret.data(), int(Ret.size()), 0, (sockaddr*)&client, (int*)&clientLength);
#else // unix
    int64_t Rcv = recvfrom(mUDPSock, Ret.data(), Ret.size(), 0, (sockaddr*)&client, (socklen_t*)&clientLength);
#endif // WIN32

    if (Rcv == -1) {
        beammp_error("(UDP) Error receiving from client! recvfrom() failed: " + GetPlatformAgnosticErrorString());
        return "";
    }
    return std::string(Ret.begin(), Ret.begin() + Rcv);
}
