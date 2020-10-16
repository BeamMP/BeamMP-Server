///
/// Created by Anonymous275 on 8/1/2020
///
#include "Security/Enc.h"
#include "Network.h"
#include "Logger.h"
#include <thread>

void TCPSend(Client*c,const std::string&Data){
    if(c == nullptr)return;
    std::string Send = "\n" + Data.substr(0,Data.find(char(0))) + "\n";
    size_t Sent = send(c->GetTCPSock(), Send.c_str(), int(Send.size()), 0);
    if (Sent == 0){
        if(c->GetStatus() > -1)c->SetStatus(-1);
    }else if (Sent < 0) {
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
    }
}
void TCPHandle(Client*c,const std::string& data){
    __try{
            c->Handler.Handle(c,data);
    }__except(1){
        c->Handler.clear();
    }
}
void TCPRcv(Client*c){
    if(c == nullptr || c->GetStatus() < 0)return;
    char buf[4096];
    int len = 4096;
    ZeroMemory(buf, len);
    int BytesRcv = recv(c->GetTCPSock(), buf, len,0);
    if (BytesRcv == 0){
        debug(Sec("(TCP) Connection closing..."));
        if(c->GetStatus() > -1)c->SetStatus(-1);
        return;
    }else if (BytesRcv < 0) {
        debug(Sec("(TCP) recv failed with error: ") + std::to_string(WSAGetLastError()));
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return;
    }
    std::string Buf(buf,BytesRcv);
    TCPHandle(c,Buf);
}
void TCPClient(Client*c){
    if(c->GetTCPSock() == -1){
        CI->RemoveClient(c);
        return;
    }
    OnConnect(c);
    while (c->GetStatus() > -1)TCPRcv(c);
    __try{
            OnDisconnect(c, c->GetStatus() == -2);
    }__except(Handle(GetExceptionInformation(),Sec("OnDisconnect"))){}
}
void InitClient(Client*c){
    std::thread NewClient(TCPClient,c);
    NewClient.detach();
}