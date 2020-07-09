///
/// Created by Anonymous275 on 7/9/2020
///
#include <random>
#include <thread>
#include "Client.hpp"
#include "../Settings.hpp"
#include <windows.h>
void VehicleParser(Client*c, std::string Packet);
int Rand(){
    std::random_device r;
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(1, 200);
    return uniform_dist(e1);
}

std::string Encrypt(std::string msg){
    if(msg.size() < 2)return msg;
    int R = (Rand()+Rand())/2,T = R;
    for(char&c : msg){
        if(R > 30)c = char(int(c) + (R-=3));
        else c = char(int(c) - (R+=4));
    }
    return char(T) + msg;
}

std::string Decrypt(std::string msg){
    int R = uint8_t(msg.at(0));
    if(msg.size() < 2 || R > 200 || R < 1)return "";
    msg = msg.substr(1);
    for(char&c : msg){
        if(R > 30)c = char(int(c) - (R-=3));
        else c = char(int(c) + (R+=4));
    }
    return msg;
}
[[noreturn]]void DLoop(){
    while(true) {
        if(IsDebuggerPresent())VehicleParser(nullptr, "");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
[[noreturn]]void SLoop(){
    std::thread D(DLoop);
    D.detach();
    int A = 0;
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (A == Beat)VehicleParser(nullptr, "");
        A = Beat;
    }
}