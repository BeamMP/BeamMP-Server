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
    std::string State = Private ? Sec("true") : Sec("false");
    std::string ret = Sec("uuid=");
    ret += Key+Sec("&players=")+std::to_string(CI->Size())+Sec("&maxplayers=")+std::to_string(MaxPlayers)+Sec("&port=")
    + std::to_string(Port) + Sec("&map=") + MapName + Sec("&private=")+State+Sec("&version=")+GetSVer()+
    Sec("&clientversion=")+GetCVer()+Sec("&name=")+ServerName+Sec("&pps=")+StatReport+Sec("&modlist=")+FileList+
    Sec("&modstotalsize=")+std::to_string(MaxModSize)+Sec("&modstotal=")+std::to_string(ModsLoaded)
    +Sec("&playerslist=")+GetPlayers()+Sec("&desc=")+ServerDesc;
    return ret;
}
void Heartbeat(){
    std::string R,T;
    while(true){
        R = GenerateCall();
        if(!CustomIP.empty())R+=Sec("&ip=")+CustomIP;
        //https://beamng-mp.com/heartbeatv2
        std::string link = Sec("https://beamng-mp.com/heartbeatv2");
        T = PostHTTP(link,R);
        if(T.find_first_not_of(Sec("20")) != std::string::npos){
            //Backend system refused server startup!
            std::this_thread::sleep_for(std::chrono::seconds(10));
            T = PostHTTP(link,R);
            if(T.find_first_not_of(Sec("20")) != std::string::npos){
                error(Sec("Backend system refused server! Check your AuthKey"));
                std::this_thread::sleep_for(std::chrono::seconds(3));
                exit(-1);
            }
        }
        //Server Authenticated
        if(T.length() == 4)info(Sec("Server authenticated"));
        R.clear();
        T.clear();
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
void HBInit(){
    std::thread HB(Heartbeat);
    HB.detach();
}