///
/// Created by Anonymous275 on 8/1/2020
///
#include "Security/Enc.h"
#include "UnixCompat.h"
#include "Compressor.h"
#include "Network.h"
#include "Logger.h"
#include <thread>


void TCPSend(Client*c,const std::string&Data){
    Assert(c);
    if(c == nullptr)return;
    // Size is BIG ENDIAN now, use only for header!
    auto Size = htonl(int32_t(Data.size()));
    std::string Send(4,0);
    memcpy(&Send[0],&Size,sizeof(Size));
    Send += Data;
    Size = int32_t(Send.size());
    int32_t Sent = 0,Temp;

    do {
        Temp = send(c->GetTCPSock(), &Send[Sent], Size - Sent, 0);
        if (Temp == 0) {
            if (c->GetStatus() > -1)c->SetStatus(-1);
            return;
        } else if (Sent < 0) {
            if (c->GetStatus() > -1)c->SetStatus(-1);
            closesocket(c->GetTCPSock());
            return;
        }
        Sent += Temp;
    }while(Sent < Size);
}

bool CheckBytes(Client*c,int32_t BytesRcv){
    Assert(c);
    if (BytesRcv == 0){
        debug(Sec("(TCP) Connection closing..."));
        if(c->GetStatus() > -1)c->SetStatus(-1);
        return false;
    }else if (BytesRcv < 0) {
        #ifdef WIN32
            debug(Sec("(TCP) recv failed with error: ") + std::to_string(WSAGetLastError()));
        #else // unix
            debug(Sec("(TCP) recv failed with error: ") + std::string(strerror(errno)));
        #endif // WIN32
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return false;
    }
    return true;
}

void TCPRcv(Client*c){
    Assert(c);
    static thread_local int32_t Header,BytesRcv,Temp;
    if(c == nullptr || c->GetStatus() < 0)return;
    #ifdef WIN32
        BytesRcv = recv(c->GetTCPSock(), reinterpret_cast<char*>(&Header), sizeof(Header),0);
    #else
        BytesRcv = recv(c->GetTCPSock(), reinterpret_cast<void*>(&Header), sizeof(Header), 0);
    #endif
    // convert back to host endianness
    Header = ntohl(Header);
#ifdef DEBUG
    //debug(std::string(__func__) + Sec(": expecting ") + std::to_string(Header) + Sec(" bytes."));
#endif // DEBUG
    if(!CheckBytes(c,BytesRcv)) {
#ifdef DEBUG
        error(std::string(__func__) + Sec(": failed on CheckBytes"));
#endif // DEBUG
        return;
    }
    // TODO: std::vector
    char* Data = new char[Header];
    Assert(Data);
    BytesRcv = 0;
    do{
        Temp = recv(c->GetTCPSock(), Data+BytesRcv, Header-BytesRcv,0);
        if(!CheckBytes(c,Temp)){
#ifdef DEBUG
            error(std::string(__func__) + Sec(": failed on CheckBytes in while(BytesRcv < Header)"));
#endif // DEBUG
            delete[] Data;
            return;
        }
#ifdef DEBUG
        //debug(std::string(__func__) + Sec(": Temp: ") + std::to_string(Temp) + Sec(", BytesRcv: ") + std::to_string(BytesRcv));
#endif // DEBUG
        BytesRcv += Temp;
    }while(BytesRcv < Header);
#ifdef DEBUG
    //debug(std::string(__func__) + Sec(": finished recv with Temp: ") + std::to_string(Temp) + Sec(", BytesRcv: ") + std::to_string(BytesRcv));
#endif // DEBUG
    std::string Ret = std::string(Data,Header);
    delete[] Data;
    if (Ret.substr(0, 4) == "ABG:") {
        Ret = DeComp(Ret.substr(4));
    }
    GParser(c,Ret);
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
