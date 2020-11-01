///
/// Created by Anonymous275 on 8/1/2020
///
#include "Security/Enc.h"
#include "Client.hpp"
#include "Settings.h"
#include "Logger.h"
#include "UnixCompat.h"
#include <fstream>

#ifdef __linux
// we need this for `struct stat`
#include <sys/stat.h>
#endif // __linux

void STCPSend(Client*c,std::string Data){
    if(c == nullptr)return;
    ssize_t BytesSent = send(c->GetTCPSock(), Data.c_str(), size_t(Data.size()), 0);
    Data.clear();
    if (BytesSent == 0){
        if(c->GetStatus() > -1)c->SetStatus(-1);
    }else if (BytesSent < 0) {
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
    }
}
void SendFile(Client*c,const std::string&Name){
    info(c->GetName()+Sec(" requesting : ")+Name.substr(Name.find_last_of('/')));
    struct stat Info{};
    if(stat(Name.c_str(), &Info) != 0){
        STCPSend(c,Sec("Cannot Open"));
        return;
    }
    std::ifstream f(Name.c_str(), std::ios::binary);
    f.seekg(0, std::ios_base::end);
    std::streampos fileSize = f.tellg();
    size_t Size = size_t(fileSize);
    size_t Sent = 0;
    size_t Diff;
    ssize_t Split = 64000;
    while(c->GetStatus() > -1 && Sent < Size){
        Diff = Size - Sent;
        if(Diff > size_t(Split)){
            std::string Data(size_t(Split),0);
            f.seekg(ssize_t(Sent), std::ios_base::beg);
            f.read(&Data[0], Split);
            STCPSend(c,Data);
            Sent += size_t(Split);
        }else{
            std::string Data(Diff,0);
            f.seekg(ssize_t(Sent), std::ios_base::beg);
            f.read(&Data[0], ssize_t(Diff));
            STCPSend(c,Data);
            Sent += Diff;
        }
    }
    f.close();
}

void Parse(Client*c,const std::string&Packet){
    if(c == nullptr || Packet.empty())return;
    char Code = Packet.at(0),SubCode = 0;
    if(Packet.length() > 1)SubCode = Packet.at(1);
    switch (Code) {
        case 'f':
            SendFile(c,Packet.substr(1));
            return;
        case 'S':
            if(SubCode == 'R'){
                debug(Sec("Sending Mod Info"));
                std::string ToSend = FileList+FileSizes;
                if(ToSend.empty())ToSend = "-";
                STCPSend(c,ToSend);
            }
            return;
        default:
            return;
    }
}
bool STCPRecv(Client*c){
    if(c == nullptr)return false;
    char buf[200];
    size_t len = 200;
    ZeroMemory(buf, len);
    ssize_t BytesRcv = recv(c->GetTCPSock(), buf, len,0);
    if (BytesRcv == 0){
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return false;
    }else if (BytesRcv < 0) {
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return false;
    }
    if(strcmp(buf,"Done") == 0)return false;
    std::string Ret(buf, size_t(BytesRcv));
    Parse(c,Ret);
    return true;
}

void SyncResources(Client*c){
    if(c == nullptr)return;
    try{
        STCPSend(c,Sec("WS"));
        while(c->GetStatus() > -1 && STCPRecv(c));
    }catch (std::exception& e){
        except(Sec("Exception! : ") + std::string(e.what()));
        c->SetStatus(-1);
    }
}
