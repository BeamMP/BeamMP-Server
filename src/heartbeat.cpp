///
/// Created by Mitch on 04/02/2020
///

#include <thread>
#include <iostream>
#include <string>
#include <chrono>
#include "logger.h"
#include "Settings.hpp"

using namespace std;

string HTTP_REQUEST(const std::string&,int);
void PostHTTP(const std::string& IP,const std::string& Fields);

void Heartbeat()
{
    string UUID = HTTP_REQUEST("https://beamng-mp.com/new-server-startup",443);
    std::string State = Private ? "true" : "false";
    while(true)
    {
        PostHTTP("https://beamng-mp.com/heartbeat","uuid="+UUID+"&players="+to_string(PlayerCount)+"&maxplayers="+to_string(MaxPlayers)+"&port="
        + to_string(Port) + "&map=" + MapName + "&private="+State+"&version="+ServerVersion+"&clientversion="+ClientVersion+"&name="+ServerName);
        std::this_thread::sleep_for (std::chrono::seconds(5));
    }
}


void HeartbeatInit()
{
    std::thread HB(Heartbeat);
    HB.detach();
}

