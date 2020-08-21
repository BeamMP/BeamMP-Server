///
/// Created by Anonymous275 on 7/31/2020
///
#include "Security/Enc.h"
#include "Curl/Http.h"
#include "Settings.h"
#include "Network.h"
#include "Logger.h"
#include <sstream>
#include <thread>
struct Hold{
    SOCKET TCPSock{};
    bool Done = false;
};
bool Send(SOCKET TCPSock,std::string Data){
    int BytesSent;
    BytesSent = send(TCPSock, Data.c_str(), int(Data.size()), 0);
    Data.clear();
    if (BytesSent <= 0)return false;
    return true;
}
std::string Rcv(SOCKET TCPSock){
    char buf[6768];
    int len = 6768;
    ZeroMemory(buf, len);
    int BytesRcv = recv(TCPSock, buf, len,0);
    if (BytesRcv <= 0)return "";
    return std::string(buf);
}
std::string GetRole(const std::string &DID){
    if(!DID.empty()){
        std::string a = HttpRequest(Sec("https://beamng-mp.com/entitlement?did=")+DID,443);
        if(!a.empty()){
            auto pos = a.find('"');
            if(pos != std::string::npos){
                return a.substr(pos+1,a.find('"',pos+1)-2);
            }else if(a == "[]")return Sec("Member");
        }
    }
    return "";
}
void Check(Hold* S){
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if(S != nullptr){
        if(!S->Done)closesocket(S->TCPSock);
    }
}
int Max(){
    int M = MaxPlayers;
    for(Client*c : CI->Clients){
        if(c != nullptr){
            if(c->GetRole() == Sec("MDEV"))M++;
        }
    }
    return M;
}
void CreateClient(SOCKET TCPSock,const std::string &Name, const std::string &DID,const std::string &Role) {
    auto *c = new Client;
    c->SetTCPSock(TCPSock);
    c->SetName(Name);
    c->SetRole(Role);
    c->SetDID(DID);
    CI->AddClient(c);
    InitClient(c);
}

std::string GenerateM(RSA*key){
    std::stringstream stream;
    stream << std::hex << key->n << "g" << key->e << "g" << RSA_E(Sec("IDC"),key);
    return stream.str();
}

void Identification(SOCKET TCPSock,Hold*S,RSA*key){
    S->TCPSock = TCPSock;
    std::thread Timeout(Check,S);
    Timeout.detach();
    std::string Name,DID,Role;
    if(!Send(TCPSock,GenerateM(key))){
        closesocket(TCPSock);
        return;
    }
    std::string Res = Rcv(TCPSock);
    std::string Ver = Rcv(TCPSock);
    S->Done = true;
    Ver = RSA_D(Ver,key);
    if(Ver.size() > 3 && Ver.substr(0,2) == Sec("VC")){
        Ver = Ver.substr(2);
        if(Ver.length() > 4 || Ver != GetCVer()){
            closesocket(TCPSock);
            return;
        }
    }else{
        closesocket(TCPSock);
        return;
    }
    Res = RSA_D(Res,key);
    if(Res.size() < 3 || Res.substr(0,2) != Sec("NR")) {
        closesocket(TCPSock);
        return;
    }
    if(Res.find(':') == std::string::npos){
        closesocket(TCPSock);
        return;
    }
    Name = Res.substr(2,Res.find(':')-2);
    DID = Res.substr(Res.find(':')+1);
    Role = GetRole(DID);
    if(Role.empty() || Role.find(Sec("Error")) != -1){
        closesocket(TCPSock);
        return;
    }
    debug(Sec("Name -> ") + Name + Sec(", Role -> ") + Role +  Sec(", ID -> ") + DID);
    for(Client*c: CI->Clients){
        if(c != nullptr){
            if(c->GetDID() == DID){
                closesocket(c->GetTCPSock());
                c->SetStatus(-2);
                break;
            }
        }
    }
    if(Role == Sec("MDEV") || CI->Size() < Max()){
        CreateClient(TCPSock,Name,DID,Role);
    }else closesocket(TCPSock);
}
void Identify(SOCKET TCPSock){
    auto* S = new Hold;
    RSA*key = GenKey();
    __try{
        Identification(TCPSock,S,key);
    }__except(1){}
    delete key;
    delete S;
}
void TCPServerMain(){
    WSADATA wsaData;
    if (WSAStartup(514, &wsaData)){
        error(Sec("Can't start Winsock!"));
        return;
    }
    SOCKET client, Listener = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_addr.S_un.S_addr = ADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);
    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR){
        error(Sec("Can't bind socket! ") + std::to_string(WSAGetLastError()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(-1);
    }
    if(Listener == -1){
        error(Sec("Invalid listening socket"));
        return;
    }
    if(listen(Listener,SOMAXCONN)){
        error(Sec("listener failed ")+ std::to_string(GetLastError()));
        return;
    }
    info(Sec("Vehicle event network online"));
    do{
        client = accept(Listener, nullptr, nullptr);
        if(client == -1){
            warn(Sec("Got an invalid client socket on connect! Skipping..."));
            continue;
        }
        std::thread ID(Identify,client);
        ID.detach();
    }while(client);

    closesocket(client);
    WSACleanup();
}