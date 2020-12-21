// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/31/2020
///

#include <Lua/LuaSystem.hpp>
#include "Security/Enc.h"
#include "UnixCompat.h"
#include "Curl/Http.h"
#include "Settings.h"
#include "Network.h"
#include "Logger.h"
#include <cstring>
#include <string>
#include <thread>
#include "Json.h"


std::string GetClientInfo(const std::string& PK) {
    if (!PK.empty()) {
       return PostHTTP("https://auth.beammp.com/pkToUser", R"({"key":")"+PK+"\"}",true);;
    }
    return "";
}

Client* CreateClient(SOCKET TCPSock) {
    auto* c = new Client;
    c->SetTCPSock(TCPSock);
    return c;
}

void ClientKick(Client* c, const std::string& R){
    info("Client kicked: " + R);
    TCPSend(c, "E" + R);
    CloseSocketProper(c->GetTCPSock());
}


void Authentication(SOCKET TCPSock) {
    DebugPrintTID();
    auto* c = CreateClient(TCPSock);

    info("Identifying new client...");
    std::string Rc = TCPRcv(c);

    if (Rc.size() > 3 && Rc.substr(0, 2) == "VC") {
        Rc = Rc.substr(2);
        if (Rc.length() > 4 || Rc != GetCVer()) {
            ClientKick(c,"Outdated Version!");
            return;
        }
    } else {
        ClientKick(c,"Invalid version header!");
        return;
    }
    TCPSend(c, "S");

    Rc = TCPRcv(c);

    if(Rc.size() > 50){
        ClientKick(c,"Invalid Key!");
        return;
    }

    Rc = GetClientInfo(Rc);
    json::Document d;
    d.Parse(Rc.c_str());
    if(Rc == "-1" || d.HasParseError()){
        ClientKick(c,"Invalid key! Please restart your game.");
        return;
    }

    if(d["username"].IsString() && d["roles"].IsString() && d["guest"].IsBool()){
        c->SetName(d["username"].GetString());
        c->SetRoles(d["roles"].GetString());
        c->isGuest = d["guest"].GetBool();
    }else{
        ClientKick(c,"Invalid authentication data!");
        return;
    }

    debug("Name -> " + c->GetName() + ", Guest -> " + std::to_string(c->isGuest) + ", Roles -> " + c->GetRoles());
    for (auto& Cl : CI->Clients) {
        if (Cl != nullptr) {
            if (Cl->GetName() == c->GetName() && Cl->isGuest == c->isGuest) {
                info("Old client (" +Cl->GetName()+ ") kicked: Reconnecting");
                CloseSocketProper(Cl->GetTCPSock());
                Cl->SetStatus(-2);
                break;
            }
        }
    }

    auto arg = std::make_unique<LuaArg>(LuaArg{{c->GetName(),c->GetRoles(),c->isGuest}});
    std::any Res = TriggerLuaEvent("onPlayerAuth",false,nullptr, std::move(arg), true);
    std::string Type = Res.type().name();
    if(Type.find("int") != std::string::npos && std::any_cast<int>(Res)){
        ClientKick(c,"you are not allowed on the server!");
        return;
    }else if(Type.find("string") != std::string::npos){
        ClientKick(c,std::any_cast<std::string>(Res));
        return;
    }
    if (CI->Size() < MaxPlayers) {
        info("Identification success");
        Client& Client = *c;
        CI->AddClient(std::move(c));
        TCPClient(&Client);
    } else ClientKick(c,"Server full!");
}

void HandleDownload(SOCKET TCPSock){
    char D;
    if(recv(TCPSock,&D,1,0) != 1){
        CloseSocketProper(TCPSock);
        return;
    }
    auto ID = uint8_t(D);
    for(auto& c : CI->Clients){
        if(c->GetID() == ID){
            c->SetDownSock(TCPSock);
        }
    }
}

void Identify(SOCKET TCPSock){
    char Code;
    if(recv(TCPSock,&Code,1,0) != 1) {
        CloseSocketProper(TCPSock);
        return;
    }
    if(Code == 'C'){
        Authentication(TCPSock);
    }else if(Code == 'D'){
        HandleDownload(TCPSock);
    }else CloseSocketProper(TCPSock);
}

void TCPServerMain() {
    DebugPrintTID();
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(514, &wsaData)) {
        error("Can't start Winsock!");
        return;
    }
    SOCKET client, Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr {};
    addr.sin_addr.S_un.S_addr = ADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);
    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        error("Can't bind socket! " + std::to_string(WSAGetLastError()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        _Exit(-1);
    }
    if (Listener == -1) {
        error("Invalid listening socket");
        return;
    }
    if (listen(Listener, SOMAXCONN)) {
        error("listener failed " + std::to_string(GetLastError()));
        return;
    }
    info("Vehicle event network online");
    do {
        try {
            client = accept(Listener, nullptr, nullptr);
            if (client == -1) {
                warn("Got an invalid client socket on connect! Skipping...");
                continue;
            }
            std::thread ID(Identify, client);
            ID.detach();
        } catch (const std::exception& e) {
            error("fatal: " + std::string(e.what()));
        }
    } while (client);

    CloseSocketProper(client);
    WSACleanup();
#else // unix
    // wondering why we need slightly different implementations of this?
    // ask ms.
    SOCKET client = -1, Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int optval = 1;
    setsockopt(Listener, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    // TODO: check optval or return value idk
    sockaddr_in addr {};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(Port));
    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) != 0) {
        error(("Can't bind socket! ") + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        _Exit(-1);
    }
    if (Listener == -1) {
        error(("Invalid listening socket"));
        return;
    }
    if (listen(Listener, SOMAXCONN)) {
        error(("listener failed ") + std::string(strerror(errno)));
        return;
    }
    info(("Vehicle event network online"));
    do {
        try {
            client = accept(Listener, nullptr, nullptr);
            if (client == -1) {
                warn(("Got an invalid client socket on connect! Skipping..."));
                continue;
            }
            std::thread ID(Identify, client);
            ID.detach();
        } catch (const std::exception& e) {
            error(("fatal: ") + std::string(e.what()));
        }
    } while (client);

    debug("all ok, arrived at " + std::string(__func__) + ":" + std::to_string(__LINE__));
    CloseSocketProper(client);
#endif // WIN32
}
