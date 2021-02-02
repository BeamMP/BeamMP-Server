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
std::string GenerateCall() {
    std::stringstream Ret;
    Ret << "uuid=" << Key << "&players=" << CI->Size()
        << "&maxplayers=" << MaxPlayers << "&port=" << Port
        << "&map=" << MapName << "&private=" << (Private ? "true" : "false")
        << "&version=" << GetSVer() << "&clientversion=" << GetCVer()
        << "&name=" << ServerName << "&pps=" << StatReport
        << "&modlist=" << FileList << "&modstotalsize=" << MaxModSize
        << "&modstotal=" << ModsLoaded << "&playerslist=" << GetPlayers()
        << "&desc=" << ServerDesc;
    return Ret.str();
}
std::string RunPromise(const std::string& host, const std::string& target, const std::unordered_map<std::string, std::string>& R, const std::string& body) {
    std::packaged_task<std::string()> task([&] { DebugPrintTIDInternal("Heartbeat_POST"); return PostHTTP(host, target, R, body, false); });
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
    std::string R;
    std::string T;

    // these are "hot-change" related variables
    static std::string LastR = "";

    static std::chrono::high_resolution_clock::time_point LastNormalUpdateTime = std::chrono::high_resolution_clock::now();
    bool isAuth = false;
    while (true) {
        R = GenerateCall();
        // a hot-change occurs when a setting has changed, to update the backend of that change.
        auto Now = std::chrono::high_resolution_clock::now();
        if (LastR == R && (Now - LastNormalUpdateTime) < std::chrono::seconds(30)) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        LastR = R;
        LastNormalUpdateTime = Now;
        if (!CustomIP.empty())
            R += "&ip=" + CustomIP;
        T = RunPromise("beammp.com", "/heartbeatv2", {}, R);

        if (T.substr(0, 2) != "20") {
            //Backend system refused server startup!
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            T = RunPromise("backup1.beammp.com", "/heartbeatv2", {}, R);
            // TODO backup2 + HTTP flag (no TSL)
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
    }
}
void HBInit() {
    std::thread HB(Heartbeat);
    HB.detach();
}
