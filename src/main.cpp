///
/// Created by Anonymous275 on 28/01/2020
///

#include <string>
#include <chrono>
#include <fstream>
#include "logger.h"
#include <algorithm>
#include "Settings.hpp"

void DebugData();
void LogInit();
void ParseConfig();
void addToLog(const std::string& Data);
//void ServerMain(int Port, int MaxClients);
void HeartbeatInit();
std::string ServerVersion = "0.47";
std::string ClientVersion = "1.46";
std::string CustomIP;
void HandleResources(std::string path);
void StatInit();
void NetMain();

int main(int argc, char* argv[]) {
    if(argc > 1){
        CustomIP = argv[1];
        size_t n = std::count(CustomIP.begin(), CustomIP.end(), '.');
        int p = CustomIP.find_first_not_of(".0123456789");
        if(p != std::string::npos || n != 3 || CustomIP.substr(0,3) == "127"){
            CustomIP.clear();
            warn("IP Specified is invalid!");
        }else info("Started with custom ip : " + CustomIP);
    }
    info("BeamMP Server Running version " + ServerVersion);
    LogInit();
    ParseConfig();
    HandleResources(Resource);
    HeartbeatInit();
    if(Debug)DebugData();
    setLoggerLevel(0); //0 for all
    /*std::thread TCPThread(TCPMain,Port);
    TCPThread.detach();*/
    //ServerMain(Port, MaxPlayers);
    if(ModsLoaded){
        info("Loaded "+std::to_string(ModsLoaded)+" Mods");
    }
    StatInit();
    NetMain();
}


void DebugData(){
    debug(std::string("Debug : ") + (Debug?"true":"false"));
    debug(std::string("Private : ") + (Private?"true":"false"));
    debug("Port : " + std::to_string(Port));
    debug("Max Cars : " + std::to_string(MaxCars));
    debug("MaxPlayers : " + std::to_string(MaxPlayers));
    debug("MapName : " + MapName);
    debug("ServerName : " + ServerName);
    debug("ServerDesc : " + ServerDesc);
    debug("File : " + Resource);
    debug("Auth Key : " + Key);
}


void LogInit(){
    std::ofstream LFS;
    LFS.open ("Server.log");
    LFS.close();
}

void addToLog(const std::string& Data){
    std::ofstream LFS;
    LFS.open ("Server.log", std::ios_base::app);
    LFS << Data.c_str();
    LFS.close();
}
