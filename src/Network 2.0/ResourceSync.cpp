///
/// Created by Anonymous275 on 6/10/2020
///
#include <string>
#include "../Settings.hpp"
#include "Client.hpp"
#include <iostream>
#include <fstream>
#include <any>

void STCPSend(Client*c,std::any Data,size_t Size){
    int BytesSent;
    if(std::string(Data.type().name()).find("string") != std::string::npos){
        auto data = std::any_cast<std::string>(Data);
        BytesSent = send(c->GetTCPSock(), data.c_str(), data.size(), 0);
        data.clear();
    }else{
        char* D = std::any_cast<char*>(Data);
        BytesSent = send(c->GetTCPSock(), D, Size, 0);
        delete[] D;
    }
    if (BytesSent == 0){
        std::cout << "(STCPS) Connection closing..." << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
    }
    else if (BytesSent < 0) {
        std::cout << "(STCPS) send failed with error: " << WSAGetLastError() << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
    }
}
void SendFile(Client*c,const std::string&Name){
    std::cout << c->GetName() << " requesting : "
    << Name.substr(Name.find_last_of('/')) << std::endl;
    struct stat Info{};
    if(stat(Name.c_str(), &Info) != 0){
        STCPSend(c,std::string("Cannot Open"),0);
        return;
    }
    std::ifstream f(Name.c_str(), std::ios::binary);
    f.seekg(0, std::ios_base::end);
    std::streampos fileSize = f.tellg();
    size_t Size = fileSize,Sent = 0,Diff;
    char* Data;
    int Split = 64000;
    while(c->GetStatus() > -1 && Sent < Size){
        Diff = Size - Sent;
        if(Diff > Split){
            Data = new char[Split];
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Split);
            STCPSend(c,Data,Split);
            Sent += Split;
        }else{
            Data = new char[Diff];
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Diff);
            STCPSend(c,Data,Diff);
            Sent += Diff;
        }
    }
    f.close();
}

void Parse(Client*c,char*data){
    std::string Packet = data;
    if(Packet.empty())return;
    char Code = Packet.at(0),SubCode = 0;
    if(Packet.length() > 1)SubCode = Packet.at(1);
    switch (Code) {
        case 'f':
            SendFile(c,Packet.substr(1));
            return;
        case 'S':
            if(SubCode == 'R'){
                std::cout << "Sending File Info" << std::endl;
                std::string ToSend = FileList+FileSizes;
                if(ToSend.empty())ToSend = "-";
                STCPSend(c,ToSend,0);
            }
            return;
    }
}
bool STCPRecv(Client*c){
    char buf[200];
    int len = 200;
    ZeroMemory(buf, len);
    int BytesRcv = recv(c->GetTCPSock(), buf, len,0);
    if (BytesRcv == 0){
        std::cout << "(STCPR) Connection closing..." << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return false;
    }
    else if (BytesRcv < 0) {
        std::cout << "(STCPR) recv failed with error: " << WSAGetLastError() << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return false;
    }
    if(strcmp(buf,"Done") == 0)return false;
    char* Ret = new char[BytesRcv];
    memcpy_s(Ret,BytesRcv,buf,BytesRcv);
    ZeroMemory(buf, len);
    Parse(c,Ret);
    delete[] Ret;
    return true;
}
void SyncResources(Client*c){
    STCPSend(c,std::string("WS"),0);
    while(c->GetStatus() > -1 && STCPRecv(c));
    c->isDownloading = false;
}