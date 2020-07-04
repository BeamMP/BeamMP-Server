///
/// Created by Anonymous275 on 5/9/2020
///
///TCP

#include "Client.hpp"
#include <iostream>
#include <WS2tcpip.h>
#include "../logger.h"
#include "../Settings.hpp"
#include <thread>
std::string HTTP_REQUEST(const std::string& IP,int port);
struct Sequence{
    SOCKET TCPSock;
    bool Done = false;
};
void CreateNewThread(Client*client);
void CreateClient(SOCKET TCPSock,const std::string &Name, const std::string &DID,const std::string &Role) {
    auto *client = new Client;
    client->SetTCPSock(TCPSock);
    client->SetName(Name);
    client->SetRole(Role);
    client->SetDID(DID);
    Clients.insert(client);
    CreateNewThread(client);
}
std::string TCPRcv(SOCKET TCPSock){
    char buf[4096];
    int len = 4096;
    ZeroMemory(buf, len);
    int BytesRcv = recv(TCPSock, buf, len,0);
    if (BytesRcv == 0){
        return "";
    }
    else if (BytesRcv < 0) {
        closesocket(TCPSock);
        return "";
    }
    return std::string(buf);
}
std::string HTTP(const std::string &DID){
    if(!DID.empty()){
        std::string a = HTTP_REQUEST("https://beamng-mp.com/entitlement?did="+DID,443);
        if(!a.empty()){
            int pos = a.find('"');
            if(pos != std::string::npos){
                return a.substr(pos+1,a.find('"',pos+1)-2);
            }else if(a == "[]")return "Member";
        }
    }
    return "";
}
void Check(Sequence* S){
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if(S != nullptr){
        if(!S->Done)closesocket(S->TCPSock);
        delete S;
    }
}
int Max(){
    int M = MaxPlayers;
    for(Client*c : Clients){
        if(c->GetRole() == "MDEV")M++;
    }
    return M;
}
void Identification(SOCKET TCPSock){
    auto* S = new Sequence;
    S->TCPSock = TCPSock;
    std::thread Timeout(Check,S);
    Timeout.detach();
    std::string Name,DID,Role,Res = TCPRcv(TCPSock),Ver = TCPRcv(TCPSock);
    S->Done = true;
    if(Ver.size() > 3 && Ver.substr(0,2) == "VC"){
        Ver = Ver.substr(2);
        if(Ver.length() > 4 || Ver != ClientVersion){
            closesocket(TCPSock);
            return;
        }
    }else{
        closesocket(TCPSock);
        return;
    }
    if(Res.size() > 3 && Res.substr(0,2) != "NR") {
        closesocket(TCPSock);
        return;
    }
    if(Res.find(':') == std::string::npos){
        closesocket(TCPSock);
        return;
    }
    Name = Res.substr(2,Res.find(':')-2);
    DID = Res.substr(Res.find(':')+1);
    Role = HTTP(DID);
    if(Role.empty() || Role.find("Error") != std::string::npos){
        closesocket(TCPSock);
        return;
    }

    for(Client*c: Clients){
        if(c->GetDID() == DID){
            closesocket(c->GetTCPSock());
            c->SetStatus(-2);
            break;
        }
    }

    if(Debug)debug("Name -> " + Name + ", Role -> " + Role +  ", ID -> " + DID);
    if(Role == "MDEV"){
        CreateClient(TCPSock,Name,DID,Role);
        return;
    }
    if(Clients.size() < Max()){
        CreateClient(TCPSock,Name,DID,Role);
    }else closesocket(TCPSock);
}

void TCPServerMain(){
    WSADATA wsaData;
    if (WSAStartup(514, &wsaData)) //2.2
    {
        std::cout << "Can't start Winsock!" << std::endl;
        return;
    }
    SOCKET client, Listener = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_addr.S_un.S_addr = ADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);


    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << "Can't bind socket! " << WSAGetLastError() << std::endl;
        return;
    }

    if(Listener == -1)
    {
        printf("Invalid socket");
        return;
    }

    if(listen(Listener,SOMAXCONN))
    {
        std::cout << "listener failed " << GetLastError();
        return;
    }
    info("Vehicle event network online");
    do{
        client = accept(Listener, nullptr, nullptr);
        if(client == -1)
        {
            std::cout << "invalid client socket" << std::endl;
            continue;
        }
        std::thread ID(Identification,client);
        ID.detach();
    }while(client);

    closesocket(client);
    WSACleanup();
}