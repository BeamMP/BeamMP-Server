#include "TTCPServer.h"
#include "TLuaEngine.h"
#include "TLuaFile.h"
#include "TResourceManager.h"
#include "TUDPServer.h"
#include <CustomAssert.h>
#include <Http.h>
#include <cstring>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
namespace json = rapidjson;

bool TCPSend(std::weak_ptr<TClient> c, const std::string& Data);
bool TCPSend(TClient& c, const std::string& Data);

TTCPServer::TTCPServer(TServer& Server, TPPSMonitor& PPSMonitor, TResourceManager& ResourceManager)
    : mServer(Server)
    , mPPSMonitor(PPSMonitor)
    , mResourceManager(ResourceManager) {
    Application::RegisterShutdownHandler([this] { mShutdown = true; });
    Start();
}

void TTCPServer::Identify(SOCKET TCPSock) {
    char Code;
    if (recv(TCPSock, &Code, 1, 0) != 1) {
        CloseSocketProper(TCPSock);
        return;
    }
    if (Code == 'C') {
        Authentication(TCPSock);
    } else if (Code == 'D') {
        HandleDownload(TCPSock);
    } else {
        CloseSocketProper(TCPSock);
    }
}

void TTCPServer::HandleDownload(SOCKET TCPSock) {
    char D;
    if (recv(TCPSock, &D, 1, 0) != 1) {
        CloseSocketProper(TCPSock);
        return;
    }
    auto ID = uint8_t(D);
    mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
        if (!ClientPtr.expired()) {
            auto c = ClientPtr.lock();
            if (c->GetID() == ID) {
                c->SetDownSock(TCPSock);
            }
        }
        return true;
    });
}

void TTCPServer::Authentication(SOCKET TCPSock) {
    auto Client = CreateClient(TCPSock);

    std::string Rc;
    info("Identifying new client...");

    Rc = TCPRcv(*Client);

    if (Rc.size() > 3 && Rc.substr(0, 2) == "VC") {
        Rc = Rc.substr(2);
        if (Rc.length() > 4 || Rc != Application::ClientVersion()) {
            ClientKick(*Client, "Outdated Version!");
            return;
        }
    } else {
        ClientKick(*Client, "Invalid version header!");
        return;
    }
    TCPSend(*Client, "S");

    Rc = TCPRcv(*Client);

    if (Rc.size() > 50) {
        ClientKick(*Client, "Invalid Key!");
        return;
    }

    if (!Rc.empty()) {
        Rc = Http::POST("auth.beammp.com", "/pkToUser", {}, R"({"key":")" + Rc + "\"}", true);
    }

    debug("Auth response: " + Rc);

    json::Document AuthResponse;
    AuthResponse.Parse(Rc.c_str());
    if (Rc == "-1" || AuthResponse.HasParseError()) {
        ClientKick(*Client, "Invalid key! Please restart your game.");
        return;
    }

    if (AuthResponse["username"].IsString() && AuthResponse["roles"].IsString() && AuthResponse["guest"].IsBool()) {
        Client->SetName(AuthResponse["username"].GetString());
        Client->SetRoles(AuthResponse["roles"].GetString());
        Client->SetIsGuest(AuthResponse["guest"].GetBool());
    } else {
        ClientKick(*Client, "Invalid authentication data!");
        return;
    }

    debug("Name -> " + Client->GetName() + ", Guest -> " + std::to_string(Client->IsGuest()) + ", Roles -> " + Client->GetRoles());
    debug("There are " + std::to_string(mServer.ClientCount()) + " known clients");
    mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
        if (!ClientPtr.expired()) {
            auto Cl = ClientPtr.lock();
            info("Client Iteration: Name -> " + Client->GetName() + ", Guest -> " + std::to_string(Client->IsGuest()) + ", Roles -> " + Client->GetRoles());
            if (Cl->GetName() == Client->GetName() && Cl->IsGuest() == Client->IsGuest()) {
                info("New client matched with current iteration");
                info("Old client (" + Cl->GetName() + ") kicked: Reconnecting");
                CloseSocketProper(Cl->GetTCPSock());
                Cl->SetStatus(-2);
                return false;
            }
        }
        return true;
    });

    auto arg = std::make_unique<TLuaArg>(TLuaArg { { Client->GetName(), Client->GetRoles(), Client->IsGuest() } });
    std::any Res = TriggerLuaEvent("onPlayerAuth", false, nullptr, std::move(arg), true);
    std::string Type = Res.type().name();
    if (Type.find("int") != std::string::npos && std::any_cast<int>(Res)) {
        ClientKick(*Client, "you are not allowed on the server!");
        return;
    } else if (Type.find("string") != std::string::npos) {
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

std::shared_ptr<TClient> TTCPServer::CreateClient(SOCKET TCPSock) {
    auto c = std::make_shared<TClient>(mServer);
    c->SetTCPSock(TCPSock);
    return c;
}

bool TTCPServer::TCPSend(TClient& c, const std::string& Data) {
    int32_t Size, Sent;
    std::string Send(4, 0);
    Size = int32_t(Data.size());
    memcpy(&Send[0], &Size, sizeof(Size));
    Send += Data;
    Sent = 0;
    Size += 4;
    do {
        int32_t Temp = send(c.GetTCPSock(), &Send[Sent], Size - Sent, 0);
        if (Temp == 0) {
            if (c.GetStatus() > -1)
                c.SetStatus(-1);
            return false;
        } else if (Temp < 0) {
            if (c.GetStatus() > -1)
                c.SetStatus(-1);
            CloseSocketProper(c.GetTCPSock());
            return false;
        }
        Sent += Temp;
    } while (Sent < Size);
    return true;
}

bool TTCPServer::CheckBytes(TClient& c, int32_t BytesRcv) {
    if (BytesRcv == 0) {
        debug("(TCP) Connection closing...");
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
        info(("Closing socket in CheckBytes, BytesRcv < 0"));
        CloseSocketProper(c.GetTCPSock());
        return false;
    }
    return true;
}

std::string TTCPServer::TCPRcv(TClient& c) {
    int32_t Header, BytesRcv = 0, Temp;
    if (c.GetStatus() < 0)
        return "";

    std::vector<char> Data(sizeof(Header));
    do {
        Temp = recv(c.GetTCPSock(), &Data[BytesRcv], 4 - BytesRcv, 0);
        if (!CheckBytes(c, Temp)) {
#ifdef DEBUG
            error(std::string(__func__) + (": failed on CheckBytes in while(BytesRcv < 4)"));
#endif // DEBUG
            return "";
        }
        BytesRcv += Temp;
    } while (size_t(BytesRcv) < sizeof(Header));
    memcpy(&Header, &Data[0], sizeof(Header));

#ifdef DEBUG
    //debug(std::string(__func__) + (": expecting ") + std::to_string(Header) + (" bytes."));
#endif // DEBUG
    if (!CheckBytes(c, BytesRcv)) {
#ifdef DEBUG
        error(std::string(__func__) + (": failed on CheckBytes"));
#endif // DEBUG
        return "";
    }
    Data.resize(Header);
    BytesRcv = 0;
    do {
        Temp = recv(c.GetTCPSock(), &Data[BytesRcv], Header - BytesRcv, 0);
        if (!CheckBytes(c, Temp)) {
#ifdef DEBUG
            error(std::string(__func__) + (": failed on CheckBytes in while(BytesRcv < Header)"));
#endif // DEBUG

            return "";
        }
#ifdef DEBUG
        //debug(std::string(__func__) + (": Temp: ") + std::to_string(Temp) + (", BytesRcv: ") + std::to_string(BytesRcv));
#endif // DEBUG
        BytesRcv += Temp;
    } while (BytesRcv < Header);
#ifdef DEBUG
    //debug(std::string(__func__) + (": finished recv with Temp: ") + std::to_string(Temp) + (", BytesRcv: ") + std::to_string(BytesRcv));
#endif // DEBUG
    std::string Ret(Data.data(), Header);

    if (Ret.substr(0, 4) == "ABG:") {
        Ret = DeComp(Ret.substr(4));
    }
#ifdef DEBUG
    //debug("Parsing from " + c->GetName() + " -> " +std::to_string(Ret.size()));
#endif

    return Ret;
}

void TTCPServer::ClientKick(TClient& c, const std::string& R) {
    info("Client kicked: " + R);
    TCPSend(c, "E" + R);
    CloseSocketProper(c.GetTCPSock());
}

void TTCPServer::TCPClient(std::weak_ptr<TClient> c) {
    // TODO: the c.expired() might cause issues here, remove if you end up here with your debugger
    if (c.expired() || c.lock()->GetTCPSock() == -1) {
        mServer.RemoveClient(c);
        return;
    }
    OnConnect(c);
    while (true) {
        if (c.expired())
            break;
        auto Client = c.lock();
        if (Client->GetStatus() <= -1) {
            break;
        }
        TServer::GlobalParser(c, TCPRcv(*Client), mPPSMonitor, UDPServer(), *this);
    }
    if (!c.expired()) {
        auto Client = c.lock();
        OnDisconnect(c, Client->GetStatus() == -2);
    } else {
        warn("client expired in TCPClient, should never happen");
    }
}

void TTCPServer::UpdatePlayers() {
    std::string Packet = ("Ss") + std::to_string(mServer.ClientCount()) + "/" + std::to_string(Application::Settings.MaxPlayers) + ":";
    mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
        if (!ClientPtr.expired()) {
            auto c = ClientPtr.lock();
            Packet += c->GetName() + ",";
        }
        return true;
    });
    Packet = Packet.substr(0, Packet.length() - 1);
    UDPServer().SendToAll(nullptr, Packet, true, true);
}

void TTCPServer::OnDisconnect(std::weak_ptr<TClient> ClientPtr, bool kicked) {
    Assert(!ClientPtr.expired());
    auto LockedClientPtr = ClientPtr.lock();
    TClient& c = *LockedClientPtr;
    info(c.GetName() + (" Connection Terminated"));
    std::string Packet;
    for (auto& v : c.GetAllCars()) {
        if (v != nullptr) {
            Packet = "Od:" + std::to_string(c.GetID()) + "-" + std::to_string(v->ID());
            UDPServer().SendToAll(&c, Packet, false, true);
        }
    }
    if (kicked)
        Packet = ("L") + c.GetName() + (" was kicked!");
    else
        Packet = ("L") + c.GetName() + (" left the server!");
    UDPServer().SendToAll(&c, Packet, false, true);
    Packet.clear();
    TriggerLuaEvent(("onPlayerDisconnect"), false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { c.GetID() } }), false);
    if (c.GetTCPSock())
        CloseSocketProper(c.GetTCPSock());
    if (c.GetDownSock())
        CloseSocketProper(c.GetDownSock());
    mServer.RemoveClient(ClientPtr);
}

int TTCPServer::OpenID() {
    int ID = 0;
    bool found;
    do {
        found = true;
        mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
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

void TTCPServer::OnConnect(std::weak_ptr<TClient> c) {
    Assert(!c.expired());
    info("Client connected");
    auto LockedClient = c.lock();
    LockedClient->SetID(OpenID());
    info("Assigned ID " + std::to_string(LockedClient->GetID()) + " to " + LockedClient->GetName());
    TriggerLuaEvent("onPlayerConnecting", false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { LockedClient->GetID() } }), false);
    SyncResources(*LockedClient);
    if (LockedClient->GetStatus() < 0)
        return;
    Respond(*LockedClient, "M" + Application::Settings.MapName, true); //Send the Map on connect
    info(LockedClient->GetName() + " : Connected");
    TriggerLuaEvent("onPlayerJoining", false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { LockedClient->GetID() } }), false);
}

void TTCPServer::SyncResources(TClient& c) {
#ifndef DEBUG
    try {
#endif
        TCPSend(c, "P" + std::to_string(c.GetID()));
        std::string Data;
        while (c.GetStatus() > -1) {
            Data = TCPRcv(c);
            if (Data == "Done")
                break;
            Parse(c, Data);
        }
#ifndef DEBUG
    } catch (std::exception& e) {
        except("Exception! : " + std::string(e.what()));
        c->SetStatus(-1);
    }
#endif
}

void TTCPServer::Parse(TClient& c, const std::string& Packet) {
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
            TCPSend(c, ToSend);
        }
        return;
    default:
        return;
    }
}

void TTCPServer::SendFile(TClient& c, const std::string& Name) {
    info(c.GetName() + " requesting : " + Name.substr(Name.find_last_of('/')));

    if (!std::filesystem::exists(Name)) {
        TCPSend(c, "CO");
        warn("File " + Name + " could not be accessed!");
        return;
    } else
        TCPSend(c, "AG");

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

    int64_t Size = std::filesystem::file_size(Name), MSize = Size / 2;

    std::thread SplitThreads[2] {
        std::thread([&] {
            SplitLoad(c, 0, MSize, false, Name);
        }),
        std::thread([&] {
            SplitLoad(c, MSize, Size, true, Name);
        })
    };

    for (auto& SplitThread : SplitThreads) {
        if (SplitThread.joinable()) {
            SplitThread.join();
        }
    }
}

void TTCPServer::SplitLoad(TClient& c, int64_t Sent, int64_t Size, bool D, const std::string& Name) {
    std::ifstream f(Name.c_str(), std::ios::binary);
    int32_t Split = 0x7735940; //125MB
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
        int64_t Diff = Size - Sent;
        if (Diff > Split) {
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Split);
            if (!TCPSendRaw(TCPSock, Data, Split)) {
                if (c.GetStatus() > -1)
                    c.SetStatus(-1);
                break;
            }
            Sent += Split;
        } else {
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Diff);
            if (!TCPSendRaw(TCPSock, Data, int32_t(Diff))) {
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

bool TTCPServer::TCPSendRaw(SOCKET C, char* Data, int32_t Size) {
    int64_t Sent = 0;
    do {
        int64_t Temp = send(C, &Data[Sent], int(Size - Sent), 0);
        if (Temp < 1) {
            info("Socket Closed! " + std::to_string(C));
            CloseSocketProper(C);
            return false;
        }
        Sent += Temp;
    } while (Sent < Size);
    return true;
}

void TTCPServer::SendLarge(TClient& c, std::string Data) {
    if (Data.length() > 400) {
        std::string CMP(Comp(Data));
        Data = "ABG:" + CMP;
    }
    TCPSend(c, Data);
}

void TTCPServer::Respond(TClient& c, const std::string& MSG, bool Rel) {
    char C = MSG.at(0);
    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
        if (C == 'O' || C == 'T' || MSG.length() > 1000) {
            SendLarge(c, MSG);
        } else {
            TCPSend(c, MSG);
        }
    } else {
        UDPServer().UDPSend(c, MSG);
    }
}

void TTCPServer::SyncClient(std::weak_ptr<TClient> c) {
    if (c.expired()) {
        return;
    }
    auto LockedClient = c.lock();
    if (LockedClient->IsSynced())
        return;
    LockedClient->SetIsSynced(true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Respond(*LockedClient, ("Sn") + LockedClient->GetName(), true);
    UDPServer().SendToAll(LockedClient.get(), ("JWelcome ") + LockedClient->GetName() + "!", false, true);
    TriggerLuaEvent(("onPlayerJoin"), false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { LockedClient->GetID() } }), false);
    bool Return = false;
    mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
        if (!ClientPtr.expired()) {
            auto client = ClientPtr.lock();
            if (client != LockedClient) {
                for (auto& v : LockedClient->GetAllCars()) {
                    if (v != nullptr) {
                        if (LockedClient->GetStatus() < 0) {
                            Return = true;
                            return false;
                        }
                        Respond(*LockedClient, v->Data(), true);
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                    }
                }
            }
        }
        return true;
    });
    if (Return) {
        return;
    }
    info(LockedClient->GetName() + (" is now synced!"));
}

void TTCPServer::SetUDPServer(TUDPServer& UDPServer) {
    mUDPServer = std::ref(UDPServer);
}

void TTCPServer::operator()() {
    while (!mUDPServer.has_value()) {
        // hard spin
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
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
            std::thread ID(&TTCPServer::Identify, this, client);
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
    SOCKET client = -1;
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
        return;
    }
    info(("Vehicle event network online"));
    do {
        try {
            if (mShutdown) {
                debug("shutdown during TCP wait for accept loop");
                break;
            }
            client = accept(Listener, nullptr, nullptr);
            if (client == -1) {
                warn(("Got an invalid client socket on connect! Skipping..."));
                continue;
            }
            std::thread ID(&TTCPServer::Identify, this, client);
            ID.detach(); // TODO: Add to a queue and attempt to join periodically
        } catch (const std::exception& e) {
            error(("fatal: ") + std::string(e.what()));
        }
    } while (client);

    debug("all ok, arrived at " + std::string(__func__) + ":" + std::to_string(__LINE__));

    CloseSocketProper(client);
#endif
}
