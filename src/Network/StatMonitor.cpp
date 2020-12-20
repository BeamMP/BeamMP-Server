// Copyright (c) 2020 Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 6/18/2020
///
#include "Client.hpp"
#include "Security/Enc.h"
#include <iostream>
#include <string>
#include <thread>
std::string StatReport;
int PPS = 0;
void Monitor() {
    int R, C = 0, V = 0;
    if (CI->Clients.empty()) {
        StatReport = "-";
        return;
    }
    for (auto& c : CI->Clients) {
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

[[noreturn]] void Stat() {
    DebugPrintTID();
    while (true) {
        Monitor();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void StatInit() {
    StatReport = "-";
    std::thread Init(Stat);
    Init.detach();
}
