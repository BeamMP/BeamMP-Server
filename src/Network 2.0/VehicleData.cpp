///
/// Created by Anonymous275 on 5/8/2020
///
///UDP

#include "Client.hpp"
#include <iostream>
#include <vector>
#include <array>
#include "../logger.h"
#include "../Settings.hpp"

struct PacketData{
    int ID;
    Client* Client;
    std::string Data;
    int Tries;
};
struct SplitData{
    int Total{};
    int ID{};
    std::set<std::pair<int,std::string>> Fragments;
};

SOCKET UDPSock;
std::set<PacketData*> DataAcks;
std::set<SplitData*> SplitPackets;
void UDPSend(Client*c,const std::string&Data){
    if(!c->isConnected())return;
    sockaddr_in Addr = c->GetUDPAddr();
    int AddrSize = sizeof(c->GetUDPAddr());
    int sendOk = sendto(UDPSock, Data.c_str(), int(Data.length()) + 1, 0, (sockaddr*)&Addr, AddrSize);
    if (sendOk == SOCKET_ERROR)error("(UDP) Send Error Code : " + std::to_string(WSAGetLastError()) + " Size : " + std::to_string(AddrSize));
}

void AckID(int ID){
    for(PacketData* p : DataAcks){
        if(p->ID == ID){
            DataAcks.erase(p);
            break;
        }
    }
}
int PacktID(){
    static int ID = -1;
    if(ID > 999999)ID = 0;
    else ID++;
    return ID;
}
int SplitID(){
    static int SID = -1;
    if(SID > 999999)SID = 0;
    else SID++;
    return SID;
}
void SendLarge(Client*c,const std::string&Data){
    int ID = PacktID();
    std::string Packet;
    if(Data.length() > 1000){
        std::string pckt = Data;
        int S = 1,Split = ceil(float(pckt.length()) / 1000);
        int SID = SplitID();
        while(pckt.length() > 1000){
            Packet = "SC"+std::to_string(S)+"/"+std::to_string(Split)+":"+std::to_string(ID)+"|"+
                     std::to_string(SID)+":"+pckt.substr(0,1000);
            DataAcks.insert(new PacketData{ID,c,Packet,1});
            UDPSend(c,Packet);
            pckt = pckt.substr(1000);
            S++;
            ID = PacktID();
        }
        Packet = "SC"+std::to_string(S)+"/"+std::to_string(Split)+":"+
                 std::to_string(ID)+"|"+std::to_string(SID)+":"+pckt;
        DataAcks.insert(new PacketData{ID,c,Packet,1});
        UDPSend(c,Packet);
    }else{
        Packet = "BD:" + std::to_string(ID) + ":" + Data;
        DataAcks.insert(new PacketData{ID,c,Packet,1});
        UDPSend(c,Packet);
    }
}


struct HandledC{
    int Pos = 0;
    Client *c{};
    std::array<int, 50> HandledIDs{};
};
std::set<HandledC*> HandledIDs;
void ResetIDs(HandledC*H){
    for(int C = 0;C < 50;C++){
        H->HandledIDs.at(C) = -1;
    }
}
HandledC*GetHandled(Client*c){
    for(HandledC*h : HandledIDs){
        if(h->c == c){
            return h;
        }
    }
    return new HandledC();
}
bool Handled(Client*c,int ID){
    bool handle = false;
    for(HandledC*h : HandledIDs){
        if(h->c == c){
            for(int id : h->HandledIDs){
                if(id == ID)return true;
            }
            if(h->Pos > 49)h->Pos = 0;
            h->HandledIDs.at(h->Pos) = ID;
            h->Pos++;
            handle = true;
        }
    }
    for(HandledC*h : HandledIDs){
        if(h->c == nullptr || !h->c->isConnected()){
            HandledIDs.erase(h);
            break;
        }
    }
    if(!handle) {
        HandledC *h = GetHandled(c);
        ResetIDs(h);
        if (h->Pos > 49)h->Pos = 0;
        h->HandledIDs.at(h->Pos) = ID;
        h->Pos++;
        h->c = c;
        HandledIDs.insert(h);
    }
    return false;
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

SplitData*GetSplit(int SplitID){
    for(SplitData* a : SplitPackets){
        if(a->ID == SplitID)return a;
    }
    auto* SP = new SplitData();
    SplitPackets.insert(SP);
    return SP;
}
void GlobalParser(Client*c, const std::string&Packet);
void HandleChunk(Client*c,const std::string&Data){
    int pos1 = int(Data.find(':'))+1,pos2 = Data.find(':',pos1),pos3 = Data.find('/');
    int pos4 = Data.find('|');
    if(pos1 == std::string::npos)return;
    int Max = stoi(Data.substr(pos3+1,pos1-pos3-2));
    int Current = stoi(Data.substr(2,pos3-2));
    int ID = stoi(Data.substr(pos1,pos4-pos1));
    int SplitID = stoi(Data.substr(pos4+1,pos2-pos4-1));
    std::string ack = "ACK:" + Data.substr(pos1,pos4-pos1);
    UDPSend(c,ack);
    if(Handled(c,ID))return;
    SplitData* SData = GetSplit(SplitID);
    SData->Total = Max;
    SData->ID = SplitID;
    SData->Fragments.insert(std::make_pair(Current,Data.substr(pos2+1)));
    if(SData->Fragments.size() == SData->Total){
        std::string ToHandle;
        for(const std::pair<int,std::string>& a : SData->Fragments){
            ToHandle += a.second;
        }
        GlobalParser(c,ToHandle);
        SplitPackets.erase(SData);
    }
}
void UDPParser(Client*c, const std::string&Packet){
    if(Packet.substr(0,4) == "ACK:"){
        AckID(stoi(Packet.substr(4)));
        return;
    }else if(Packet.substr(0,3) == "BD:"){
        int pos = Packet.find(':',4);
        int ID = stoi(Packet.substr(3,pos-3));
        std::string pckt = "ACK:" + std::to_string(ID);
        UDPSend(c,pckt);
        if(!Handled(c,ID)) {
            pckt = Packet.substr(pos + 1);
            GlobalParser(c, pckt);
        }
        return;
    }else if(Packet.substr(0,2) == "SC"){

        HandleChunk(c,Packet);
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

    DataAcks.clear();
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
        for (PacketData* p : DataAcks){
            if (p->Client == nullptr || p->Client->GetTCPSock() == -1) {
                DataAcks.erase(p);
                break;
            }
            if(p->Tries < 20){
                UDPSend(p->Client,p->Data);
                p->Tries++;
            }else{
                DataAcks.erase(p);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
void StartLoop(){
    std::thread Ack(LOOP);
    Ack.detach();
}
