///
/// Created by Anonymous275 on 7/28/2020
///
#include "Security/Enc.h"
#include "Curl/Http.h"
#include "Client.hpp"
#include "Settings.h"
#include "Logger.h"
#include <thread>
#include <chrono>

void WebsocketInit();
std::string GetPlayers(){
    std::string Return;
    for(Client* c : CI->Clients){
        if(c != nullptr){
            Return += c->GetName() + ";";
        }
    }
    return Return;
}
std::string GenerateCall(){
    std::string State = Private ? "true" : "false";
    std::string ret = "uuid=";
    ret += Key+"&players="+std::to_string(CI->Size())+"&maxplayers="+std::to_string(MaxPlayers)+"&port="
    + std::to_string(Port) + "&map=" + MapName + "&private="+State+"&version="+GetSVer()+
    "&clientversion="+GetCVer()+"&name="+ServerName+"&pps="+StatReport+"&modlist="+FileList+
    "&modstotalsize="+std::to_string(MaxModSize)+"&modstotal="+std::to_string(ModsLoaded)
    +"&playerslist="+GetPlayers()+"&desc="+ServerDesc;
    return ret;
}
void Heartbeat(){
    DebugPrintTID();
    std::string R,T;
    bool isAuth = false;
    while(true){
        R = GenerateCall();
        if(!CustomIP.empty())R+="&ip="+CustomIP;
        std::string link = Sec("https://beammp.com/heartbeatv2");
        T = PostHTTP(link,R);

        if(T.find_first_not_of(Sec("20")) != std::string::npos){
            //Backend system refused server startup!
            std::this_thread::sleep_for(std::chrono::seconds(10));
            std::string Backup = Sec("https://backup1.beammp.com/heartbeatv2");
            T = PostHTTP(Backup,R);
            if(T.find_first_not_of(Sec("20")) != std::string::npos) {
                error(Sec("Backend system refused server! Check your AuthKey"));
                std::this_thread::sleep_for(std::chrono::seconds(3));
                exit(-1);
            }
        }
        //Server Authenticated
        if(T.length() == 4)info(Sec("Server authenticated"));
        R.clear();
        T.clear();
        if(!isAuth){
            WebsocketInit();
            isAuth = true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
void HBInit(){
    std::thread HB(Heartbeat);
    HB.detach();
}
