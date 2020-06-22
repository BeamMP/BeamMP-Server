///
/// Created by Anonymous275 on 6/18/2020
///
#include "Client.hpp"
#include <string>
#include <thread>
std::string StatReport = "-";
int PPS = 0;
[[noreturn]] void Monitor(){
    int R,C,V=0;
    while(true){
        if(Clients.empty()){
            StatReport = "-";
        }else{
            C = 0;
            for(Client *c : Clients){
                if(c->GetCarCount() > 0){
                    C++;
                    V += c->GetCarCount();
                }
            }
            if(C == 0 || PPS == 0){
                StatReport = "-";
            }else{
                R = (PPS/C)/V;
                StatReport = std::to_string(R);
                PPS = 0;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void StatInit(){
    std::thread Init(Monitor);
    Init.detach();
}
