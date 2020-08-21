///
/// Created by Anonymous275 on 8/1/2020
///
#include "Security/Enc.h"
#include "Network.h"
#include "Logger.h"
#include <thread>

void TCPSend(Client*c,const std::string&Data){
    if(c == nullptr)return;
    int BytesSent = send(c->GetTCPSock(), Data.c_str(), int(Data.length())+1, 0);
    if (BytesSent == 0){
        if(c->GetStatus() > -1)c->SetStatus(-1);
    }else if (BytesSent < 0) {
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
    }
}
void TCPRcv(Client*c){
    if(c == nullptr)return;
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
    GParser(c, Buf);
}
void TCPClient(Client*c){
    if(c->GetTCPSock() == -1){
        CI->RemoveClient(c);
        return;
    }
    info(Sec("Client connected"));
    OnConnect(c);
    while (c->GetStatus() > -1)TCPRcv(c);
    info(c->GetName() + Sec(" Connection Terminated"));
    OnDisconnect(c, c->GetStatus() == -2);
}
void InitClient(Client*c){
    std::thread NewClient(TCPClient,c);
    NewClient.detach();
}