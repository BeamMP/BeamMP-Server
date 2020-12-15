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
    //c->SetRoles(Roles);
    //c->isGuest = Guest;
    //c->SetName(Name);
    return c;
}

void ClientKick(Client* c, const std::string& R){
    info("Client kicked: " + R);
    TCPSend(c, "E" + R);
    CloseSocketProper(c->GetTCPSock());
}


void Identification(SOCKET TCPSock) {
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
        ClientKick(c,"Invalid key!");
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
            if (Cl->GetName() == c->GetName()) {
                info("Old client (" +Cl->GetName()+ ") kicked: Reconnecting");
                CloseSocketProper(Cl->GetTCPSock());
                Cl->SetStatus(-2);
                break;
            }
        }
    }
    auto arg = std::make_unique<LuaArg>(LuaArg{{c->GetName(),c->GetRoles(),c->isGuest}});
    int Res = TriggerLuaEvent("onPlayerAuth",false,nullptr, std::move(arg), true);
    if(Res){
        ClientKick(c,"you are not allowed on the server!");
        return;
    }
    if (CI->Size() < MaxPlayers) {
        info("Identification success");
        Client& Client = *c;
        CI->AddClient(std::move(c));
        InitClient(&Client);
    } else ClientKick(c,"Server full!");
}

void TCPServerMain() {
    DebugPrintTID();
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(514, &wsaData)) {
        error(Sec("Can't start Winsock!"));
        return;
    }
    SOCKET client, Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr {};
    addr.sin_addr.S_un.S_addr = ADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);
    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        error(Sec("Can't bind socket! ") + std::to_string(WSAGetLastError()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        _Exit(-1);
    }
    if (Listener == -1) {
        error(Sec("Invalid listening socket"));
        return;
    }
    if (listen(Listener, SOMAXCONN)) {
        error(Sec("listener failed ") + std::to_string(GetLastError()));
        return;
    }
    info(Sec("Vehicle event network online"));
    do {
        try {
            client = accept(Listener, nullptr, nullptr);
            if (client == -1) {
                warn(Sec("Got an invalid client socket on connect! Skipping..."));
                continue;
            }
            std::thread ID(Identification, client);
            ID.detach();
        } catch (const std::exception& e) {
            error(Sec("fatal: ") + std::string(e.what()));
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
        error(Sec("Can't bind socket! ") + std::string(strerror(errno)));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        _Exit(-1);
    }
    if (Listener == -1) {
        error(Sec("Invalid listening socket"));
        return;
    }
    if (listen(Listener, SOMAXCONN)) {
        error(Sec("listener failed ") + std::string(strerror(errno)));
        return;
    }
    info(Sec("Vehicle event network online"));
    do {
        try {
            client = accept(Listener, nullptr, nullptr);
            if (client == -1) {
                warn(Sec("Got an invalid client socket on connect! Skipping..."));
                continue;
            }
            std::thread ID(Identify, client);
            ID.detach();
        } catch (const std::exception& e) {
            error(Sec("fatal: ") + std::string(e.what()));
        }
    } while (client);

    debug("all ok, arrived at " + std::string(__func__) + ":" + std::to_string(__LINE__));
    CloseSocketProper(client);
#endif // WIN32
}
