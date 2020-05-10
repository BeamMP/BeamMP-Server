///
/// Created by Anonymous275 on 5/8/2020
///
///UDP

#include "Client.hpp"
#include <iostream>
#include <vector>
#include "../logger.h"
#include "../Settings.hpp"

SOCKET UDPSock;

void UDPSend(Client*c,const std::string&Data){
    sockaddr_in Addr = c->GetUDPAddr();
    int AddrSize = sizeof(c->GetUDPAddr());
    int sendOk = sendto(UDPSock, Data.c_str(), int(Data.length()) + 1, 0, (sockaddr*)&Addr, AddrSize);
    if (sendOk == SOCKET_ERROR)error("(UDP) Send Error! Code : " + std::to_string(WSAGetLastError()));
}

std::string UDPRcvFromClient(sockaddr_in& client){
    char buf[4096];
    int clientLength = sizeof(client);
    ZeroMemory(&client, clientLength);
    ZeroMemory(buf, 4096);
    int bytesIn = recvfrom(UDPSock, buf, 4096, 0, (sockaddr*)&client, &clientLength);
    if (bytesIn == -1)
    {
        error("(UDP) Error receiving from Client! Code : " + std::to_string(WSAGetLastError()));
        return "";
    }
    return std::string(buf);
}

void GlobalParser(Client*c, const std::string&Packet);

[[noreturn]] void UDPServerMain(){

    WSADATA data;

    if (WSAStartup(514, &data)) //2.2
    {

        std::cout << "Can't start Winsock!" << std::endl;
        //return;
    }

    UDPSock = socket(AF_INET, SOCK_DGRAM, 0);

    // Create a server hint structure for the server
    sockaddr_in serverAddr{};
    serverAddr.sin_addr.S_un.S_addr = ADDR_ANY; //Any Local
    serverAddr.sin_family = AF_INET; // Address format is IPv4
    serverAddr.sin_port = htons(Port); // Convert from little to big endian

    // Try and bind the socket to the IP and port
    if (bind(UDPSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cout << "Can't bind socket! " << WSAGetLastError() << std::endl;
        //return;
    }
    info("Vehicle data network online on port "+std::to_string(Port)+" with a Max of "+std::to_string(MaxPlayers)+" Clients");
    while (true)
    {
        sockaddr_in client{};
        std::string Data = UDPRcvFromClient(client); //Receives any data from Socket
        int Pos = Data.find(':');
        if(Data.empty() || Pos < 0 || Pos > 2)continue;
        /*char clientIp[256];
        ZeroMemory(clientIp, 256); ///Code to get IP we don't need that yet
        inet_ntop(AF_INET, &client.sin_addr, clientIp, 256);*/

        uint8_t ID = Data.at(0)-1;
        for(Client*c : Clients){
            if(c->GetID() == ID){
                c->SetUDPAddr(client);
                GlobalParser(c,Data.substr(2));
            }
        }
    }
    ///UDPSendToClient(c->GetUDPAddr(), sizeof(c->GetUDPAddr()), Data);
    /*closesocket(UDPSock);
    WSACleanup();
    return;*/
}