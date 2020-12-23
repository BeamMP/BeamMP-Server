// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 5/8/2020
///

#include "Client.hpp"
#include "Compressor.h"
#include "Logger.h"
#include "Network.h"
#include "Security/Enc.h"
#include "Settings.h"
#include "UnixCompat.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>

SOCKET UDPSock;
void UDPSend(Client* c, std::string Data) {
    Assert(c);
    if (c == nullptr || !c->isConnected || c->GetStatus() < 0)
        return;
    sockaddr_in Addr = c->GetUDPAddr();
    socklen_t AddrSize = sizeof(c->GetUDPAddr());
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
    if (sendOk == -1) {
        debug(("(UDP) Send Failed Code : ") + std::to_string(WSAGetLastError()));
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
    } else if (sendOk == 0) {
        debug(("(UDP) sendto returned 0"));
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
    }
#else // unix
    if (sendOk == -1) {
        debug(("(UDP) Send Failed Code : ") + std::string(strerror(errno)));
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
    } else if (sendOk == 0) {
        debug(("(UDP) sendto returned 0"));
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
    }
#endif // WIN32
}

void SendLarge(Client* c, std::string Data) {
    Assert(c);
    if (Data.length() > 400) {
        std::string CMP(Comp(Data));
        Data = "ABG:" + CMP;
    }
    TCPSend(c, Data);
}

std::string UDPRcvFromClient(sockaddr_in& client) {
    size_t clientLength = sizeof(client);
    ZeroMemory(&client, clientLength);
    std::string Ret(10240, 0);
    int64_t Rcv = recvfrom(UDPSock, &Ret[0], 10240, 0, (sockaddr*)&client, (socklen_t*)&clientLength);
    if (Rcv == -1) {
#ifdef WIN32
        error(("(UDP) Error receiving from Client! Code : ") + std::to_string(WSAGetLastError()));
#else // unix
        error(("(UDP) Error receiving from Client! Code : ") + std::string(strerror(errno)));
#endif // WIN32
        return "";
    }
    return Ret.substr(0, Rcv);
}

void UDPParser(Client* c, std::string Packet) {
    if (Packet.find("Zp") != std::string::npos && Packet.size() > 500) {
        abort();
    }
    Assert(c);
    if (Packet.substr(0, 4) == "ABG:") {
        Packet = DeComp(Packet.substr(4));
    }
    GParser(c, Packet);
}

[[noreturn]] void UDPServerMain() {
#ifdef WIN32
    WSADATA data;
    if (WSAStartup(514, &data)) {
        error(("Can't start Winsock!"));
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
        error(("Can't bind socket!") + std::to_string(WSAGetLastError()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        _Exit(-1);
        //return;
    }

    info(("Vehicle data network online on port ") + std::to_string(Port) + (" with a Max of ") + std::to_string(MaxPlayers) + (" Clients"));
    while (true) {
        try {
            sockaddr_in client {};
            std::string Data = UDPRcvFromClient(client); //Receives any data from Socket
            auto Pos = Data.find(':');
            if (Data.empty() || Pos > 2)
                continue;
            /*char clientIp[256];
            ZeroMemory(clientIp, 256); ///Code to get IP we don't need that yet
            inet_ntop(AF_INET, &client.sin_addr, clientIp, 256);*/
            uint8_t ID = Data.at(0) - 1;
            for (auto& c : CI->Clients) {
                if (c != nullptr && c->GetID() == ID) {
                    c->SetUDPAddr(client);
                    c->isConnected = true;
                    UDPParser(c.get(), Data.substr(2));
                }
            }
        } catch (const std::exception& e) {
            error(("fatal: ") + std::string(e.what()));
        }
    }
    /*CloseSocketProper(UDPSock);
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
        error(("Can't bind socket!") + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        _Exit(-1);
        //return;
    }

    info(("Vehicle data network online on port ") + std::to_string(Port) + (" with a Max of ") + std::to_string(MaxPlayers) + (" Clients"));
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
            for (auto& c : CI->Clients) {
                if (c != nullptr && c->GetID() == ID) {
                    c->SetUDPAddr(client);
                    c->isConnected = true;
                    UDPParser(c.get(), Data.substr(2));
                }
            }
        } catch (const std::exception& e) {
            error(("fatal: ") + std::string(e.what()));
        }
    }
    /*CloseSocketProper(UDPSock); // TODO: Why not this? We did this in TCPServerMain?
    return;
     */
#endif // WIN32
}
