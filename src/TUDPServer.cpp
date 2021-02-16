#include "TUDPServer.h"
#include "CustomAssert.h"
#include <any>
#include <cstring>

TUDPServer::TUDPServer(TServer& Server, TPPSMonitor& PPSMonitor)
    : mServer(Server)
    , mPPSMonitor(PPSMonitor) {
}

void TUDPServer::operator()() {
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
    while (true) {
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
                if (!ClientPtr.expired()) {
                    auto Client = ClientPtr.lock();
                    if (Client->GetID() == ID) {
                        Client->SetUDPAddr(client);
                        Client->SetConnected(true);
                        UDPParser(*Client, Data.substr(2));
                    }
                }
                return true;
            });
        } catch (const std::exception& e) {
            error(("fatal: ") + std::string(e.what()));
        }
    }
}
void TUDPServer::UDPParser(TClient& Client, std::string Packet) {
    if (Packet.find("Zp") != std::string::npos && Packet.size() > 500) {
        abort();
    }
    if (Packet.substr(0, 4) == "ABG:") {
        Packet = DeComp(Packet.substr(4));
    }
    if (Packet.empty()) {
        return;
    }
    std::any Res;
    char Code = Packet.at(0);

    //V to Z
    if (Code <= 90 && Code >= 86) {
        mPPSMonitor.IncrementInternalPPS();
        SendToAll(&Client, Packet, false, false);
        return;
    }
    switch (Code) {
    case 'H': // initial connection
#ifdef DEBUG
        debug(std::string("got 'H' packet: '") + Packet + "' (" + std::to_string(Packet.size()) + ")");
#endif
        SyncClient(Client);
        return;
    case 'p':
        Respond(Client, ("p"), false);
        UpdatePlayers();
        return;
    case 'O':
        if (Packet.length() > 1000) {
            debug(("Received data from: ") + Client->GetName() + (" Size: ") + std::to_string(Packet.length()));
        }
        ParseVeh(Client, Packet);
        return;
    case 'J':
#ifdef DEBUG
        debug(std::string(("got 'J' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        SendToAll(Client, Packet, false, true);
        return;
    case 'C':
#ifdef DEBUG
        debug(std::string(("got 'C' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        if (Packet.length() < 4 || Packet.find(':', 3) == std::string::npos)
            break;
        Res = TriggerLuaEvent("onChatMessage", false, nullptr, std::make_unique<LuaArg>(LuaArg { { Client->GetID(), Client->GetName(), Packet.substr(Packet.find(':', 3) + 1) } }), true);
        if (std::any_cast<int>(Res))
            break;
        SendToAll(nullptr, Packet, true, true);
        return;
    case 'E':
#ifdef DEBUG
        debug(std::string(("got 'E' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        HandleEvent(Client, Packet);
        return;
    default:
        return;
    }
}

void TUDPServer::SendToAll(TClient* c, const std::string& Data, bool Self, bool Rel) {
    if (!Self)
        Assert(c);
    char C = Data.at(0);
    mServer.ForEachClient([&](std::weak_ptr<TClient> ClientPtr) -> bool {
        if (!ClientPtr.expired()) {
            auto Client = ClientPtr.lock();
            if (Self || Client.get() != c) {
                if (Client->IsSynced()) {
                    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
                        if (C == 'O' || C == 'T' || Data.length() > 1000)
                            SendLarge(*Client, Data);
                        else
                            TCPSend(*Client, Data);
                    } else
                        UDPSend(*Client, Data);
                }
            }
        }
        return true;
    });
}

void TUDPServer::UDPSend(TClient& Client, std::string Data) const {
    if (!Client.IsConnected() || Client.GetStatus() < 0) {
#ifdef DEBUG
        debug(Client.GetName() + ": !IsConnected() or GetStatus() < 0");
#endif // DEBUG
        return;
    }
    sockaddr_in Addr = Client.GetUDPAddr();
    socklen_t AddrSize = sizeof(Client.GetUDPAddr());
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

    sendOk = sendto(mUDPSock, Data.c_str(), len, 0, (sockaddr*)&Addr, AddrSize);
#ifdef WIN32
    if (sendOk == -1) {
        debug(("(UDP) Send Failed Code : ") + std::to_string(WSAGetLastError()));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
    } else if (sendOk == 0) {
        debug(("(UDP) sendto returned 0"));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
    }
#else // unix
    if (sendOk == -1) {
        debug(("(UDP) Send Failed Code : ") + std::string(strerror(errno)));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
    } else if (sendOk == 0) {
        debug(("(UDP) sendto returned 0"));
        if (Client.GetStatus() > -1)
            Client.SetStatus(-1);
    }
#endif // WIN32
}

std::string TUDPServer::UDPRcvFromClient(sockaddr_in& client) const {
    size_t clientLength = sizeof(client);
    std::array<char, 1024> Ret {};
    int64_t Rcv = recvfrom(mUDPSock, Ret.data(), Ret.size(), 0, (sockaddr*)&client, (socklen_t*)&clientLength);
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
