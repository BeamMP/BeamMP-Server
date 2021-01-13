// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/28/2020
///
#include "Client.hpp"
#include "Curl/Http.h"
#include "Logger.h"
#include "Settings.h"
#include <chrono>
#include <future>
#include <sstream>
#include <thread>
#include <unordered_map>

void WebsocketInit();
std::string GetPlayers() {
    std::string Return;
    for (auto& c : CI->Clients) {
        if (c != nullptr) {
            Return += c->GetName() + ";";
        }
    }
    return Return;
}
std::unordered_map<std::string, std::string> GenerateCall() {
    return { { "uuid", Key },
        { "players", std::to_string(CI->Size()) },
        { "maxplayers", std::to_string(MaxPlayers) },
        { "port", std::to_string(Port) },
        { "map", MapName },
        { "private", (Private ? "true" : "false") },
        { "version", GetSVer() },
        { "clientversion", GetCVer() },
        { "name", ServerName },
        { "pps", StatReport },
        { "modlist", FileList },
        { "modstotalsize", std::to_string(MaxModSize) },
        { "modstotal", std::to_string(ModsLoaded) },
        { "playerslist", GetPlayers() },
        { "desc", ServerDesc } };
}
std::string RunPromise(const std::string& host, const std::string& target, const std::unordered_map<std::string, std::string>& R) {
    std::packaged_task<std::string()> task([&] { return PostHTTP(host, target, R, "", false); });
    std::future<std::string> f1 = task.get_future();
    std::thread t(std::move(task));
    t.detach();
    auto status = f1.wait_for(std::chrono::seconds(15));
    if (status != std::future_status::timeout)
        return f1.get();
    error("Backend system Timeout please try again later");
    return "";
}

[[noreturn]] void Heartbeat() {
    DebugPrintTID();
    std::unordered_map<std::string, std::string> R;
    std::string T;
    bool isAuth = false;
    while (true) {
        R = GenerateCall();
        if (!CustomIP.empty())
            R.insert({ "ip", CustomIP });
        T = RunPromise("beammp.com", "/heartbeatv2", R);

        if (T.substr(0, 2) != "20") {
            //Backend system refused server startup!
            std::this_thread::sleep_for(std::chrono::seconds(10));
            T = RunPromise("backup1.beammp.com", "/heartbeatv2", R);
            if (T.substr(0, 2) != "20") {
                warn("Backend system refused server! Server might not show in the public list");
            }
        }
        //Server Authenticated
        if (!isAuth) {
            WebsocketInit();
            if (T.length() == 4)
                info(("Authenticated!"));
            else
                info(("Resumed authenticated session!"));
            isAuth = true;
        }
        //std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
void HBInit() {
    std::thread HB(Heartbeat);
    HB.detach();
}
