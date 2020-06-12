///
/// Created by Anonymous275 on 6/10/2020
///
#include <string>
#include "../Settings.hpp"
#include "Client.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <any>


void GrabRole(Client*c);

void STCPSend(Client*c,const std::any& Data,size_t Size){
    char *MSG;
    if(std::string(Data.type().name()).find("string") != std::string::npos){
        MSG = (char*)(std::any_cast<std::string>(Data).c_str());
    }else MSG = std::any_cast<char*>(Data);
    if(Size == 0)Size = strlen(MSG)+1;
    int BytesSent = send(c->GetTCPSock(), MSG, Size, 0);
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
    char* Data = new char[Size];
    f.seekg(0, std::ios_base::beg);
    f.read(Data, fileSize);
    f.close();
    char* Chunk;
    int Split = 64000;
    while(c->GetStatus() > -1 && Sent < Size){
        Diff = Size - Sent;
        if(Diff > Split){
            Chunk = new char[Split];
            memcpy_s(Chunk,Split,Data+Sent,Split);
            STCPSend(c,Chunk,Split);
            Sent += Split;
        }else{
            Chunk = new char[Diff];
            memcpy_s(Chunk,Diff,Data+Sent,Diff);
            STCPSend(c,Chunk,Diff);
            Sent += Diff;
        }
    }
    delete[] Data;
    delete[] Chunk;
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
                STCPSend(c,FileList+FileSizes,0);
            }
            return;
        case 'N':
            if(SubCode == 'R'){
                c->SetName(Packet.substr(2,Packet.find(':')-2));
                c->SetDID(Packet.substr(Packet.find(':')+1));
                GrabRole(c);
            }
            std::cout << "Name : " << c->GetName() << std::endl;
            return;
    }
}
bool STCPRecv(Client*c){
    char buf[200];
    int len = 200;
    ZeroMemory(buf, len);
    int BytesRcv = recv(c->GetTCPSock(), buf, len,0);
    if (BytesRcv == 0){
        std::cout << "(TCP) Connection closing..." << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
        return false;
    }
    else if (BytesRcv < 0) {
        std::cout << "(TCP) recv failed with error: " << WSAGetLastError() << std::endl;
        if(c->GetStatus() > -1)c->SetStatus(-1);
        closesocket(c->GetTCPSock());
        return false;
    }
    if(strcmp(buf,"Done") == 0)return false;
    char* Ret = new char[BytesRcv];
    memcpy_s(Ret,BytesRcv,buf,BytesRcv);
    ZeroMemory(buf, len);
    Parse(c,Ret);
    return true;
}
void SyncResources(Client*c){
    while(c->GetStatus() > -1 && STCPRecv(c));
}