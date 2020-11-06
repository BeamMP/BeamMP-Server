///
/// Created by Anonymous275 on 6/18/2020
///
#include "Security/Enc.h"
#include "Client.hpp"
#include <iostream>
#include <string>
#include <thread>
std::string StatReport;
int PPS = 0;
void Monitor() {
    int R, C = 0, V = 0;
    if (CI->Clients.empty()){
        StatReport = "-";
        return;
    }
    for (Client *c : CI->Clients) {
        if (c != nullptr && c->GetCarCount() > 0) {
            C++;
            V += c->GetCarCount();
        }
    }
    if (C == 0 || PPS == 0) {
        StatReport = "-";
    } else {
        R = (PPS / C) / V;
        StatReport = std::to_string(R);
    }
    PPS = 0;
}

[[noreturn]]void Stat(){
    DebugPrintTID();
    while(true){
        Monitor();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void StatInit(){
    StatReport = "-";
    std::thread Init(Stat);
    Init.detach();
}
