///
/// Created by Anonymous275 on 5/8/2020
///
///UDP
#include "Client.hpp"
#include "Compressor.h"
#include "Logger.h"
#include "Network.h"
#include "Security/Enc.h"
#include "Settings.h"
#include "UnixCompat.h"
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>
int FC(const std::string& s, const std::string& p, int n);
struct PacketData {
    int ID;
    ::Client* Client;
    std::string Data;
    int Tries;
};
struct SplitData {
    int Total {};
    int ID {};
    std::set<std::pair<int, std::string>> Fragments;
};

SOCKET UDPSock;
std::set<PacketData*> DataAcks;
std::set<SplitData*> SplitPackets;
void UDPSend(Client* c, std::string Data) {
    Assert(c);
    if (c == nullptr || !c->isConnected || c->GetStatus() < 0)
        return;
    sockaddr_in Addr = c->GetUDPAddr();
    socklen_t AddrSize = sizeof(c->GetUDPAddr());
    Data = Data.substr(0, Data.find(char(0)));
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

    sendOk = sendto(UDPSock, Data.c_str(), len, 0, (sockaddr*)&Addr, AddrSize);
#ifdef WIN32
    if (sendOk != 0) {
        debug(Sec("(UDP) Send Failed Code : ") + std::to_string(WSAGetLastError()));
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
    }
#else // unix
    if (sendOk != 0) {
        debug(Sec("(UDP) Send Failed Code : ") + std::string(strerror(errno)));
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
    }
#endif // WIN32
}

void AckID(int ID) {
    for (PacketData* p : DataAcks) {
        if (p != nullptr && p->ID == ID) {
            DataAcks.erase(p);
            break;
        }
    }
}
int PacktID() {
    static int ID = -1;
    if (ID > 999999)
        ID = 0;
    else
        ID++;
    return ID;
}
int SplitID() {
    static int SID = -1;
    if (SID > 999999)
        SID = 0;
    else
        SID++;
    return SID;
}
void SendLarge(Client* c, std::string Data) {
    Assert(c);
    Data = Data.substr(0, Data.find(char(0)));
    int ID = PacktID();
    std::string Packet;
    if (Data.length() > 1000) {
        std::string pckt = Data;
        int S = 1, Split = int(ceil(float(pckt.length()) / 1000));
        int SID = SplitID();
        while (pckt.length() > 1000) {
            Packet = "SC|" + std::to_string(S) + "|" + std::to_string(Split) + "|" + std::to_string(ID) + "|" + std::to_string(SID) + "|" + pckt.substr(0, 1000);
            DataAcks.insert(new PacketData { ID, c, Packet, 1 });
            UDPSend(c, Packet);
            pckt = pckt.substr(1000);
            S++;
            ID = PacktID();
        }
        Packet = "SC|" + std::to_string(S) + "|" + std::to_string(Split) + "|" + std::to_string(ID) + "|" + std::to_string(SID) + "|" + pckt;
        DataAcks.insert(new PacketData { ID, c, Packet, 1 });
        UDPSend(c, Packet);
    } else {
        Packet = "BD:" + std::to_string(ID) + ":" + Data;
        DataAcks.insert(new PacketData { ID, c, Packet, 1 });
        UDPSend(c, Packet);
    }
}
struct HandledC {
    size_t Pos = 0;
    Client* c = nullptr;
    std::array<int, 100> HandledIDs = { -1 };
};
std::set<HandledC*> HandledIDs;
void ResetIDs(HandledC* H) {
    for (size_t C = 0; C < 100; C++) {
        H->HandledIDs.at(C) = -1;
    }
}
HandledC* GetHandled(Client* c) {
    Assert(c);
    for (HandledC* h : HandledIDs) {
        if (h->c == c) {
            return h;
        }
    }
    return new HandledC();
}
bool Handled(Client* c, int ID) {
    Assert(c);
    bool handle = false;
    for (HandledC* h : HandledIDs) {
        if (h->c == c) {
            for (int id : h->HandledIDs) {
                if (id == ID)
                    return true;
            }
            if (h->Pos > 99)
                h->Pos = 0;
            h->HandledIDs.at(h->Pos) = ID;
            h->Pos++;
            handle = true;
        }
    }
    for (HandledC* h : HandledIDs) {
        if (h->c == nullptr || !h->c->isConnected) {
            HandledIDs.erase(h);
            break;
        }
    }
    if (!handle) {
        HandledC* h = GetHandled(c);
        ResetIDs(h);
        if (h->Pos > 99)
            h->Pos = 0;
        h->HandledIDs.at(h->Pos) = ID;
        h->Pos++;
        h->c = c;
        HandledIDs.insert(h);
    }
    return false;
}
std::string UDPRcvFromClient(sockaddr_in& client) {
    size_t clientLength = sizeof(client);
    ZeroMemory(&client, clientLength);
    std::string Ret(10240, 0);
    int64_t Rcv = recvfrom(UDPSock, &Ret[0], 10240, 0, (sockaddr*)&client, (socklen_t*)&clientLength);
    if (Rcv == -1) {
#ifdef WIN32
        error(Sec("(UDP) Error receiving from Client! Code : ") + std::to_string(WSAGetLastError()));
#else // unix
        error(Sec("(UDP) Error receiving from Client! Code : ") + std::string(strerror(errno)));
#endif // WIN32
        return "";
    }
    return Ret;
}

SplitData* GetSplit(int SplitID) {
    for (SplitData* a : SplitPackets) {
        if (a->ID == SplitID)
            return a;
    }
    auto* SP = new SplitData();
    SplitPackets.insert(SP);
    return SP;
}
void HandleChunk(Client* c, const std::string& Data) {
    Assert(c);
    int pos = FC(Data, "|", 5);
    if (pos == -1)
        return;
    std::stringstream ss(Data.substr(0, size_t(pos++)));
    std::string t;
    int I = -1;
    //Current Max ID SID
    std::vector<int> Num(4, 0);
    while (std::getline(ss, t, '|')) {
        if (I >= 0)
            Num.at(size_t(I)) = std::stoi(t);
        I++;
    }
    std::string ack = "TRG:" + std::to_string(Num.at(2));
    UDPSend(c, ack);
    if (Handled(c, Num.at(2))) {
        return;
    }
    std::string Packet = Data.substr(size_t(pos));
    SplitData* SData = GetSplit(Num.at(3));
    SData->Total = Num.at(1);
    SData->ID = Num.at(3);
    SData->Fragments.insert(std::make_pair(Num.at(0), Packet));
    if (SData->Fragments.size() == size_t(SData->Total)) {
        std::string ToHandle;
        for (const std::pair<int, std::string>& a : SData->Fragments) {
            ToHandle += a.second;
        }
        GParser(c, ToHandle);
        SplitPackets.erase(SData);
        delete SData;
        SData = nullptr;
    }
}
void UDPParser(Client* c, std::string Packet) {
    Assert(c);
    if (Packet.substr(0, 4) == "ABG:") {
        Packet = DeComp(Packet.substr(4));
    }
    if (Packet.substr(0, 4) == "TRG:") {
        std::string pkt = Packet.substr(4);
        if (Packet.find_first_not_of("0123456789") == std::string::npos) {
            AckID(stoi(Packet));
        }
        return;
    } else if (Packet.substr(0, 3) == "BD:") {
        auto pos = Packet.find(':', 4);
        int ID = stoi(Packet.substr(3, pos - 3));
        std::string pkt = "TRG:" + std::to_string(ID);
        UDPSend(c, pkt);
        if (!Handled(c, ID)) {
            pkt = Packet.substr(pos + 1);
            GParser(c, pkt);
        }
        return;
    } else if (Packet.substr(0, 2) == "SC") {
        HandleChunk(c, Packet);
        return;
    }
    GParser(c, Packet);
}
void LOOP() {
    DebugPrintTID();
    while (UDPSock != -1) {
        for (PacketData* p : DataAcks) {
            if (p != nullptr) {
                if (p->Client == nullptr || p->Client->GetTCPSock() == -1) {
                    DataAcks.erase(p);
                    break;
                }
                if (p->Tries < 15) {
                    UDPSend(p->Client, p->Data);
                    p->Tries++;
                } else {
                    DataAcks.erase(p);
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}
[[noreturn]] void UDPServerMain() {
#ifdef WIN32
    WSADATA data;
    if (WSAStartup(514, &data)) {
        error(Sec("Can't start Winsock!"));
        //return;
    }

    UDPSock = socket(AF_INET, SOCK_DGRAM, 0);
    // Create a server hint structure for the server
    sockaddr_in serverAddr {};
    serverAddr.sin_addr.S_un.S_addr = ADDR_ANY; //Any Local
    serverAddr.sin_family = AF_INET; // Address format is IPv4
    serverAddr.sin_port = htons(Port); // Convert from little to big endian

    // Try and bind the socket to the IP and port
    if (bind(UDPSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        error(Sec("Can't bind socket!") + std::to_string(WSAGetLastError()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1);
        //return;
    }

    DataAcks.clear();
    std::thread Ack(LOOP);
    Ack.detach();

    info(Sec("Vehicle data network online on port ") + std::to_string(Port) + Sec(" with a Max of ") + std::to_string(MaxPlayers) + Sec(" Clients"));
    while (true) {
        sockaddr_in client {};
        std::string Data = UDPRcvFromClient(client); //Receives any data from Socket
        auto Pos = Data.find(':');
        if (Data.empty() || Pos < 0 || Pos > 2)
            continue;
        /*char clientIp[256];
        ZeroMemory(clientIp, 256); ///Code to get IP we don't need that yet
        inet_ntop(AF_INET, &client.sin_addr, clientIp, 256);*/
        uint8_t ID = Data.at(0) - 1;
        for (Client* c : CI->Clients) {
            if (c != nullptr && c->GetID() == ID) {
                c->SetUDPAddr(client);
                c->isConnected = true;
                UDPParser(c, Data.substr(2));
            }
        }
    }
    /*closesocket(UDPSock);
    WSACleanup();
    return;*/
#else // unix
    UDPSock = socket(AF_INET, SOCK_DGRAM, 0);
    // Create a server hint structure for the server
    sockaddr_in serverAddr {};
    serverAddr.sin_addr.s_addr = INADDR_ANY; //Any Local
    serverAddr.sin_family = AF_INET; // Address format is IPv4
    serverAddr.sin_port = htons(uint16_t(Port)); // Convert from little to big endian

    // Try and bind the socket to the IP and port
    if (bind(UDPSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        error(Sec("Can't bind socket!") + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1);
        //return;
    }

    DataAcks.clear();
    std::thread Ack(LOOP);
    Ack.detach();

    info(Sec("Vehicle data network online on port ") + std::to_string(Port) + Sec(" with a Max of ") + std::to_string(MaxPlayers) + Sec(" Clients"));
    while (true) {
        sockaddr_in client {};
        std::string Data = UDPRcvFromClient(client); //Receives any data from Socket
        size_t Pos = Data.find(':');
        if (Data.empty() || Pos > 2)
            continue;
        /*char clientIp[256];
        ZeroMemory(clientIp, 256); ///Code to get IP we don't need that yet
        inet_ntop(AF_INET, &client.sin_addr, clientIp, 256);*/
        uint8_t ID = uint8_t(Data.at(0)) - 1;
        for (Client* c : CI->Clients) {
            if (c != nullptr && c->GetID() == ID) {
                c->SetUDPAddr(client);
                c->isConnected = true;
                UDPParser(c, Data.substr(2));
            }
        }
    }
    /*closesocket(UDPSock); // TODO: Why not this? We did this in TCPServerMain?
    return;
     */
#endif // WIN32
}
