///
/// Created by Anonymous275 on 8/1/2020
///
#include "Security/Enc.h"
#include "Network.h"
#include "Logger.h"
#include "UnixCompat.h"
#include <thread>

void TCPSend(Client*c,const std::string&Data){
    Assert(c);
    if(c == nullptr)return;
    std::string Send = "\n" + Data.substr(0,Data.find(char(0))) + "\n";
#ifdef WIN32
    int Sent;
    int len = static_cast<int>(Send.size());
#else
    int64_t Sent;
    size_t len = Send.size();
#endif // WIN32
    Sent = send(c->GetTCPSock(), Send.c_str(), len, 0);
    if (Sent == 0){
        if(c->GetStatus() > -1)c->SetStatus(-1);
    }else if (Sent < 0) {
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
    }
}
void TCPHandle(Client*c,const std::string& data){
    Assert(c);
#ifdef WIN32
    __try{
#endif // WIN32
            c->Handler.Handle(c,data);
#ifdef WIN32
    }__except(1){
        c->Handler.clear();
    }
#endif // WIN32
}
void TCPRcv(Client*c){
    Assert(c);
    if(c == nullptr || c->GetStatus() < 0)return;
    #define len 4096
    char buf[len];
    ZeroMemory(buf, len);
    int64_t BytesRcv = recv(c->GetTCPSock(), buf, len,0);
    #undef len
    if (BytesRcv == 0){
        debug(Sec("(TCP) Connection closing..."));
        if(c->GetStatus() > -1)c->SetStatus(-1);
        return;
    }else if (BytesRcv < 0) {
#ifdef WIN32
        debug(Sec("(TCP) recv failed with error: ") + std::to_string(WSAGetLastError()));
#else // unix
        debug(Sec("(TCP) recv failed with error: ") + std::string(strerror(errno)));
#endif // WIN32
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return;
    }
    std::string Buf(buf,(size_t(BytesRcv)));
    TCPHandle(c,Buf);
}
void TCPClient(Client*c){
    DebugPrintTID();
    Assert(c);
    if(c->GetTCPSock() == -1){
        CI->RemoveClient(c);
        return;
    }
    OnConnect(c);
    while (c->GetStatus() > -1)TCPRcv(c);
    OnDisconnect(c, c->GetStatus() == -2);
}
void InitClient(Client*c){
    std::thread NewClient(TCPClient,c);
    NewClient.detach();
}
