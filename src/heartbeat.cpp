///
/// Created by Mitch on 04/02/2020
///

#include <thread>
#include <iostream>
#include <chrono>
#include "logger.h"
#include "Settings.hpp"
#include "Network 2.0/Client.hpp"
extern std::string StatReport;
std::string HTTP_REQUEST(const std::string&,int);
std::string PostHTTP(const std::string& IP,const std::string& Fields);
std::string HTA(const std::string& hex)
{
    std::string ascii;
    for (size_t i = 0; i < hex.length(); i += 2)
    {
        std::string part = hex.substr(i, 2);
        char ch = stoul(part, nullptr, 16);
        ascii += ch;
    }
    return ascii;
}
std::string GetPlayers(){
    std::string Return;
    for(Client* c : Clients){
        Return += c->GetName() + ";";
    }
    return Return;
}
void Heartbeat(){
    std::string State,R,T;
    while(true){
        State = Private ? "true" : "false";
        R = "uuid="+Key+"&players="+std::to_string(Clients.size())+"&maxplayers="+std::to_string(MaxPlayers)+"&port="
            + std::to_string(Port) + "&map=" + MapName + "&private="+State+"&version="+ServerVersion+
            "&clientversion="+ClientVersion+"&name="+ServerName+"&pps="+StatReport+"&modlist="+FileList+
            "&modstotalsize="+std::to_string(MaxModSize)+"&modstotal="+std::to_string(ModsLoaded)
            +"&playerslist="+GetPlayers()+"&desc="+ServerDesc;
        if(!CustomIP.empty())R+="&ip="+CustomIP;
        //https://beamng-mp.com/heartbeatv2
        T = PostHTTP(HTA("68747470733a2f2f6265616d6e672d6d702e636f6d2f6865617274626561747632"),R);
        if(T.find_first_not_of("20") != std::string::npos){
            //Backend system refused server startup!
            std::this_thread::sleep_for(std::chrono::seconds(10));
            T = PostHTTP(HTA("68747470733a2f2f6265616d6e672d6d702e636f6d2f6865617274626561747632"),R);
            if(T.find_first_not_of("20") != std::string::npos){
                error(HTA("4261636b656e642073797374656d20726566757365642073657276657221"));
                std::this_thread::sleep_for(std::chrono::seconds(3));
                exit(-1);
            }
        }
        //Server Authenticated
        if(T.length() == 4)info(HTA("5365727665722061757468656e746963617465642077697468206261636b656e64"));
        R.clear();
        T.clear();
        State.clear();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}


void HeartbeatInit()
{
    std::thread HB(Heartbeat);
    HB.detach();
}
