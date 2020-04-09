///
/// Created by Mitch on 04/02/2020
///

#include <thread>
#include <iostream>
#include <string>
#include <chrono>
using namespace std;

string HTTP_REQUEST(const std::string&,int);
void PostHTTP(const std::string& IP,const std::string& Fields);

int PlayerCount;
int Port;
int MaxPlayers;
string MapName;
string ServerName;
string Resource;
string ServerVersion;


void Heartbeat()
{
    string UUID = HTTP_REQUEST("https://beamng-mp.com/new-server-startup",443);
    std::cout << "UUID GEN : " << UUID << std::endl;
    while(true)
    {
        //"name=daniel&project=curl"
        //player maxplayers port map private version
        PostHTTP("https://beamng-mp.com/heartbeat","d.uuid="+UUID+"&d.players="+to_string(PlayerCount)+"&d.maxplayers="+to_string(MaxPlayers)+"&d.port="
        + to_string(Port) + "&d.map=" + MapName + "&d.private=false"+"&d.version="+ServerVersion);

        std::this_thread::sleep_for (std::chrono::seconds(30));
    }
}


void HeartbeatInit()
{
    /// Make initial connection to backend services to get UUID, then call Heartbeat()
    std::thread HB(Heartbeat);
    HB.detach();
}

