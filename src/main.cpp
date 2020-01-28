//
// Created by Anonymous275 on 28.01.2020.
//

#include "main.h"
#include <iostream>
#include <string>
#include "enet/enet.h"
#include "network.h"
#include "logger.h"

using namespace std; //nameSpace STD

void DebugData();
void ParseConfig();

bool Debug = false;
int Port = 30813;
string MapName = "levels/gridmap/level.json";
string ServerName = "BeamNG-MP FTW";

//Entry
int main() {
    ParseConfig();
    if(Debug){ //checks if debug is on
        DebugData(); //Prints Debug Data
    }
    setLoggerLevel("ALL");
    startRUDP("localhost", 30814);
}


void DebugData(){
    cout << "Debug : true" << "\n";
    cout << "Port : " << Port << "\n";
    cout << "MapName : " << MapName << "\n";
    cout << "ServerName : " << ServerName << "\n";
}

void SetMainValues(bool D, int P,string Name,string serverName){
    Debug = D;
    Port = P;
    MapName = Name;
    ServerName = serverName;
}

