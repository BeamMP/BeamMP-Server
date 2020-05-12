///
/// Created by Anonymous275 on 5/8/2020
///
///UDP

#include "Client.hpp"
#include <iostream>
#include <vector>
#include <tuple>

#include "../logger.h"
#include "../Settings.hpp"

SOCKET UDPSock;

std::set<std::tuple<int,Client*,std::string>> BigDataAcks;

void UDPSend(Client*c,const std::string&Data){
    if(!c->isConnected())return;
    sockaddr_in Addr = c->GetUDPAddr();
    int AddrSize = sizeof(c->GetUDPAddr());
    int sendOk = sendto(UDPSock, Data.c_str(), int(Data.length()) + 1, 0, (sockaddr*)&Addr, AddrSize);
    if (sendOk == SOCKET_ERROR)error("(UDP) Send Error Code : " + std::to_string(WSAGetLastError()) + " Size : " + std::to_string(AddrSize));
}

void AckID(int ID){
    if(BigDataAcks.empty())return;
    for(std::tuple<int,Client*,std::string> a : BigDataAcks){
        if(get<0>(a) == ID)BigDataAcks.erase(a);
    }
}

void TCPSendLarge(Client*c,const std::string&Data){
    static int ID = 0;
    std::string Header = "BD:" + std::to_string(ID) + ":";
    //BigDataAcks.insert(std::make_tuple(ID,c,Header+Data));
    UDPSend(c,Header+Data);
    if(ID > 483647)ID = 0;
    else ID++;
}

std::string UDPRcvFromClient(sockaddr_in& client){
    char buf[10240];
    int clientLength = sizeof(client);
    ZeroMemory(&client, clientLength);
    ZeroMemory(buf, 10240);
    int bytesIn = recvfrom(UDPSock, buf, 10240, 0, (sockaddr*)&client, &clientLength);
    if (bytesIn == -1)
    {
        error("(UDP) Error receiving from Client! Code : " + std::to_string(WSAGetLastError()));
        return "";
    }
    return std::string(buf);
}

void GlobalParser(Client*c, const std::string&Packet);

void UDPParser(Client*c, const std::string&Packet){
    if(Packet.substr(0,4) == "ACK:"){
        AckID(stoi(Packet.substr(4)));
        return;
    }else if(Packet.substr(0,3) == "BD:"){
        int pos = Packet.find(':',4);
        std::string pckt = "ACK:" + Packet.substr(3,pos-3);
        UDPSend(c,pckt);
        pckt = Packet.substr(pos+1);
        GlobalParser(c,pckt);
        return;
    }
    GlobalParser(c,Packet);
}

void StartLoop();
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

    BigDataAcks.clear();
    StartLoop();

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
                c->SetConnected(true);
                UDPParser(c,Data.substr(2));
            }
        }
    }
    ///UDPSendToClient(c->GetUDPAddr(), sizeof(c->GetUDPAddr()), Data);
    /*closesocket(UDPSock);
    WSACleanup();
    return;*/
}
#include <thread>
void LOOP(){
    while(UDPSock != -1) {
        for (std::tuple<int, Client *, std::string> a : BigDataAcks) {
            if (get<1>(a)->GetTCPSock() == -1) {
                BigDataAcks.erase(a);
                continue;
            }
            //UDPSend(get<1>(a), get<2>(a));
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
void StartLoop(){
    std::thread Ack(LOOP);
    Ack.detach();
}