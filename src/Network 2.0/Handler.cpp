///
/// Created by Anonymous275 on 5/9/2020
///
///TCP

#include "Client.hpp"
#include <iostream>
#include <thread>


void TCPSend(Client*c,const std::string&Data){
    int BytesSent = send(c->GetTCPSock(), Data.c_str(), int(Data.length())+1, 0);
    if (BytesSent == 0){
        std::cout << "(TCP) Connection closing..." << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
    }
    else if (BytesSent < 0) {
        std::cout << "(TCP) send failed with error: " << WSAGetLastError() << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
    }
}

void GlobalParser(Client*c, const std::string&Packet);
void TCPRcv(Client*c){
    char buf[4096];
    int len = 4096;
    ZeroMemory(buf, len);
    int BytesRcv = recv(c->GetTCPSock(), buf, len,0);
    if (BytesRcv == 0){
        std::cout << "(TCP) Connection closing..." << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
    }
    else if (BytesRcv < 0) {
        std::cout << "(TCP) recv failed with error: " << WSAGetLastError() << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
    }
    GlobalParser(c, std::string(buf));
}

void OnConnect(Client*c);
void OnDisconnect(Client*c,bool Timed);
void TCPClient(Client*client){
    if(client->GetTCPSock() == -1)return;
    std::cout << "Client connected" << std::endl;
    OnConnect(client);
    while (client->GetStatus() > -1){
        TCPRcv(client);
    }
    //OnDisconnect
    OnDisconnect(client, client->GetStatus() == -2);
    std::cout << "Client Terminated" << std::endl;
}

void CreateNewThread(Client*client){
    std::thread NewClient(TCPClient,client);
    NewClient.detach();
}
