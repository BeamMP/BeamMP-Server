#include "TNetwork.h"
#include "Client.h"
#include <CustomAssert.h>
#include <Http.h>
#include <array>
#include <cstring>

TNetwork::TNetwork(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager)
    : mServer(Server)
    , mPPSMonitor(PPSMonitor)
    , mResourceManager(ResourceManager) {
    Application::RegisterShutdownHandler([&] {
        debug("Kicking all players due to shutdown");
        Server.ForEachClient([&](std::weak_ptr<TClient> client) -> bool {
            if (!client.expired()) {
                ClientKick(*client.lock(), "Server shutdown");
            }
            return true;
        });
    });
    Application::RegisterShutdownHandler([&] {
        if (mUDPThread.joinable()) {
            debug("shutting down TCPServer");
            mShutdown = true;
            mUDPThread.detach();
            debug("shut down TCPServer");
        }
    });
    Application::RegisterShutdownHandler([&] {
        if (mTCPThread.joinable()) {
            debug("shutting down TCPServer");
            mShutdown = true;
            mTCPThread.detach();
            debug("shut down TCPServer");
        }
    });
    mTCPThread = std::thread(&TNetwork::TCPServerMain, this);
    mUDPThread = std::thread(&TNetwork::UDPServerMain, this);
}

void TNetwork::UDPServerMain() {
    RegisterThread("UDPServer");
#ifdef WIN32
    WSADATA data;
    if (WSAStartup(514, &data)) {
        error(("Can't start Winsock!"));
        //return;
    }

    mUDPSock = socket(AF_INET, SOCK_DGRAM, 0);
    // Create a server hint structure for the server
    sockaddr_in serverAddr {};
    serverAddr.sin_addr.S_un.S_addr = ADDR_ANY; //Any Local
    serverAddr.sin_family = AF_INET; // Address format is IPv4
    serverAddr.sin_port = htons(Application::Settings.Port); // Convert from little to big endian

    // Try and bind the socket to the IP and port
    if (bind(mUDPSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        error(("Can't bind socket!") + std::to_string(WSAGetLastError()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1);
        //return;
    }
#else // unix
    mUDPSock = socket(AF_INET, SOCK_DGRAM, 0);
    // Create a server hint structure for the server
    sockaddr_in serverAddr {};
    serverAddr.sin_addr.s_addr = INADDR_ANY; //Any Local
    serverAddr.sin_family = AF_INET; // Address format is IPv4
    serverAddr.sin_port = htons(uint16_t(Application::Settings.Port)); // Convert from little to big endian

    // Try and bind the socket to the IP and port
    if (bind(mUDPSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        error(("Can't bind socket!") + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1);
        //return;
    }
#endif

    info(("Vehicle data network online on port ") + std::to_string(Application::Settings.Port) + (" with a Max of ")
        + std::to_string(Application::Settings.MaxPlayers) + (" Clients"));
    while (!mShutdown) {
        try {
            sockaddr_in client {};
            std::string Data = UDPRcvFromClient(client); //Receives any data from Socket
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
            error(("fatal: ") + std::string(e.what()));
        }
    }
}

void TNetwork::TCPServerMain() {
    RegisterThread("TCPServer");
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(514, &wsaData)) {
        error("Can't start Winsock!");
        return;
    }
    SOCKET client, Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr {};
    addr.sin_addr.S_un.S_addr = ADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Application::Settings.Port);
    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        error("Can't bind socket! " + std::to_string(WSAGetLastError()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1);
    }
    if (Listener == -1) {
        error("Invalid listening socket");
        return;
    }

    if (listen(Listener, SOMAXCONN)) {
        error("listener failed " + std::to_string(GetLastError()));
        //TODO Fix me leak for Listener socket
        return;
    }
    info("Vehicle event network online");
    do {
        try {
            client = accept(Listener, nullptr, nullptr);
            if (client == -1) {
                warn("Got an invalid client socket on connect! Skipping...");
                continue;
            }
            std::thread ID(&TNetwork::Identify, this, client);
            ID.detach();
        } catch (const std::exception& e) {
            error("fatal: " + std::string(e.what()));
        }
    } while (client);

    CloseSocketProper(client);
    WSACleanup();
#else // unix
    // wondering why we need slightly different implementations of this?
    // ask ms.
    TConnection client {};
    SOCKET Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int optval = 1;
    setsockopt(Listener, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    // TODO: check optval or return value idk
    sockaddr_in addr {};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(Application::Settings.Port));
    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) != 0) {
        error(("Can't bind socket! ") + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1);
    }
    if (Listener == -1) {
        error(("Invalid listening socket"));
        return;
    }
    if (listen(Listener, SOMAXCONN)) {
        error(("listener failed ") + std::string(strerror(errno)));
        //TODO fix me leak Listener
        return;
    }
    info(("Vehicle event network online"));
    do {
        try {
            if (mShutdown) {
                debug("shutdown during TCP wait for accept loop");
                break;
            }
            client.SockAddrLen = sizeof(client.SockAddr);
            client.Socket = accept(Listener, &client.SockAddr, &client.SockAddrLen);
            if (client.Socket == -1) {
                warn(("Got an invalid client socket on connect! Skipping..."));
                continue;
            }
            std::thread ID(&TNetwork::Identify, this, client);
            ID.detach(); // TODO: Add to a queue and attempt to join periodically
        } catch (const std::exception& e) {
            error(("fatal: ") + std::string(e.what()));
        }
    } while (client.Socket);

    debug("all ok, arrived at " + std::string(__func__) + ":" + std::to_string(__LINE__));

    CloseSocketProper(client.Socket);
#endif
}

#undef GetObject //Fixes Windows

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
    Client->SetIdentifier("ip", inet_ntoa(reinterpret_cast<const struct sockaddr_in*>(&ClientConnection.SockAddr)->sin_addr));

    std::string Rc;
    info("Identifying new ClientConnection...");

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
    int ResponseCode = -1;
    if (!Rc.empty()) {
        Rc = Http::POST(Application::GetBackendUrlForAuth(), Target, {}, RequestString, true, &ResponseCode);
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
            error("Backend returned invalid auth response format. This should never happen.");
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

    debug("Name -> " + Client->GetName() + ", Guest -> " + std::to_string(Client->IsGuest()) + ", Roles -> " + Client->GetRoles());
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

    auto arg = std::make_unique<TLuaArg>(TLuaArg { { Client->GetName(), Client->GetRoles(), Client->IsGuest() } });
    std::any Res = TriggerLuaEvent("onPlayerAuth", false, nullptr, std::move(arg), true);
    if (Res.type() == typeid(int) && std::any_cast<int>(Res)) {
        ClientKick(*Client, "you are not allowed on the server!");
        return;
    } else if (Res.type() == typeid(std::string)) {
        ClientKick(*Client, std::any_cast<std::string>(Res));
        return;
    }

    if (mServer.ClientCount() < size_t(Application::Settings.MaxPlayers)) {
        info("Identification success");
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
#ifdef WIN32
        int32_t Temp = send(c.GetTCPSock(), &Send[Sent], Size - Sent, 0);
#else //WIN32
        int32_t Temp = send(c.GetTCPSock(), &Send[Sent], Size - Sent, MSG_NOSIGNAL);
#endif //WIN32
        if (Temp == 0) {
            debug("send() == 0: " + std::string(std::strerror(errno)));
            if (c.GetStatus() > -1)
                c.SetStatus(-1);
            return false;
        } else if (Temp < 0) {
            debug("send() < 0: " + std::string(std::strerror(errno))); //TODO fix it was spamming yet everyone stayed on the server
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
        trace("(TCP) Connection closing...");
        if (c.GetStatus() > -1)
            c.SetStatus(-1);
        return false;
    } else if (BytesRcv < 0) {
#ifdef WIN32
        debug(("(TCP) recv failed with error: ") + std::to_string(WSAGetLastError()));
#else // unix
        debug(("(TCP) recv failed with error: ") + std::string(strerror(errno)));
#endif // WIN32
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
        warn("Client " + c.GetName() + " (" + std::to_string(c.GetID()) + ") sent header of >100MB - assuming malicious intent and disconnecting the client.");
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
    info("Client kicked: " + R);
    if (!TCPSend(c, "E" + R)) {
        // TODO handle
    }
    c.SetStatus(-2);

    if (c.GetTCPSock())
        CloseSocketProper(c.GetTCPSock());

    if (c.GetDownSock())
        CloseSocketProper(c.GetDownSock());
}
void TNetwork::Looper(const std::weak_ptr<TClient>& c) {
    while (!c.expired()) {
        auto Client = c.lock();
        if (Client->GetStatus() < 0) {
            debug("client status < 0, breaking client loop");
            break;
        }
        if (!Client->IsSyncing() && Client->IsSynced() && Client->MissedPacketQueueSize() != 0) {
            //debug("sending " + std::to_string(Client->MissedPacketQueueSize()) + " queued packets");
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
                // debug("sending a missed packet: " + QData);
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
            debug("client status < 0, breaking client loop");
            break;
        }

        auto res = TCPRcv(*Client);
        if (res == "") {
            debug("TCPRcv error, break client loop");
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
        warn("client expired in TCPClient, should never happen");
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
    Assert(!ClientPtr.expired());
    auto LockedClientPtr = ClientPtr.lock();
    TClient& c = *LockedClientPtr;
    info(c.GetName() + (" Connection Terminated"));
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
    TriggerLuaEvent(("onPlayerDisconnect"), false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { c.GetID() } }), false);
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
    Assert(!c.expired());
    info("Client connected");
    auto LockedClient = c.lock();
    LockedClient->SetID(OpenID());
    info("Assigned ID " + std::to_string(LockedClient->GetID()) + " to " + LockedClient->GetName());
    TriggerLuaEvent("onPlayerConnecting", false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { LockedClient->GetID() } }), false);
    SyncResources(*LockedClient);
    if (LockedClient->GetStatus() < 0)
        return;
    (void)Respond(*LockedClient, "M" + Application::Settings.MapName, true); //Send the Map on connect
    info(LockedClient->GetName() + " : Connected");
    TriggerLuaEvent("onPlayerJoining", false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { LockedClient->GetID() } }), false);
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
        error("Exception! : " + std::string(e.what()));
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
            debug("Sending Mod Info");
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
    info(c.GetName() + " requesting : " + UnsafeName.substr(UnsafeName.find_last_of('/')));

    if (!fs::path(UnsafeName).has_filename()) {
        if (!TCPSend(c, "CO")) {
            // TODO: handle
        }
        warn("File " + UnsafeName + " is not a file!");
        return;
    }
    auto FileName = fs::path(UnsafeName).filename().string();
    FileName = Application::Settings.Resource + "/Client/" + FileName;

    if (!std::filesystem::exists(FileName)) {
        if (!TCPSend(c, "CO")) {
            // TODO: handle
        }
        warn("File " + UnsafeName + " could not be accessed!");
        return;
    }

    if (!TCPSend(c, "AG")) {
        // TODO: handle
    }

    ///Wait for connections
    int T = 0;
    while (c.GetDownSock() < 1 && T < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        T++;
    }

    if (c.GetDownSock() < 1) {
        error("Client doesn't have a download socket!");
        if (c.GetStatus() > -1)
            c.SetStatus(-1);
        return;
    }

    size_t Size = size_t(std::filesystem::file_size(FileName)), MSize = Size / 2;

    std::thread SplitThreads[2] {
        std::thread([&] {
            SplitLoad(c, 0, MSize, false, FileName);
        }),
        std::thread([&] {
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
    uint32_t Split = 0x7735940; //125MB
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
    info("Split load Socket " + std::to_string(TCPSock));
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
        intmax_t Temp = send(socket, &Data[Sent], int(Size - Sent), 0);
        if (Temp < 1) {
            info("Socket Closed! " + std::to_string(socket));
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

    TriggerLuaEvent(("onPlayerJoin"), false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { LockedClient->GetID() } }), false);
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
    info(LockedClient->GetName() + (" is now synced!"));
    return true;
}

void TNetwork::SendToAll(TClient* c, const std::string& Data, bool Self, bool Rel) {
    if (!Self)
        Assert(c);
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
                        //ret = SendLarge(*Client, Data);
                    } else {
                        Client->EnqueuePacket(Data);
                        //ret = TCPSend(*Client, Data);
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
#ifdef WIN32
    if (sendOk == -1) {
        debug(("(UDP) Send Failed Code : ") + std::to_string(WSAGetLastError()));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
        return false;
    } else if (sendOk == 0) {
        debug(("(UDP) sendto returned 0"));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
        return false;
    }
#else // unix
    if (sendOk == -1) {
        debug(("(UDP) Send Failed Code : ") + std::string(strerror(errno)));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
        return false;
    } else if (sendOk == 0) {
        debug(("(UDP) sendto returned 0"));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
        return false;
    }
#endif // WIN32
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
#ifdef WIN32
        error(("(UDP) Error receiving from Client! Code : ") + std::to_string(WSAGetLastError()));
#else // unix
        error(("(UDP) Error receiving from Client! Code : ") + std::string(strerror(errno)));
#endif // WIN32
        return "";
    }
    return std::string(Ret.begin(), Ret.begin() + Rcv);
}