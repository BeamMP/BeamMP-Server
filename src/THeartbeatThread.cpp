#include "THeartbeatThread.h"

#include "Http.h"
#include "SocketIO.h"

THeartbeatThread::THeartbeatThread() {
}

void THeartbeatThread::operator()() {
    std::string Body;
    std::string T;

    // these are "hot-change" related variables
    static std::string Last = "";

    static std::chrono::high_resolution_clock::time_point LastNormalUpdateTime = std::chrono::high_resolution_clock::now();
    bool isAuth = false;
    while (true) {
        Body = GenerateCall();
        // a hot-change occurs when a setting has changed, to update the backend of that change.
        auto Now = std::chrono::high_resolution_clock::now();
        if (Last == Body && (Now - LastNormalUpdateTime) < std::chrono::seconds(30)) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        Last = Body;
        LastNormalUpdateTime = Now;
        if (!Application::Settings.CustomIP.empty())
            Body += "&ip=" + Application::Settings.CustomIP;

        T = Http::POST("backend.beammp.com", "/heartbeat", {}, Body, false);

        if (T.substr(0, 2) != "20") {
            //Backend system refused server startup!
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            T = Http::POST("backend.beammp.com", "/heartbeat", {}, Body, false);
            // TODO backup2 + HTTP flag (no TSL)
            if (T.substr(0, 2) != "20") {
                warn("Backend system refused server! Server might not show in the public list");
                debug("server returned \"" + T + "\"");
                isAuth = false;
            }
        }

        if (!isAuth) {
            if (T == "2000") {
                info(("Authenticated!"));
                isAuth = true;
            } else if (T == "200") {
                info(("Resumed authenticated session!"));
                isAuth = true;
            }
        }

        SocketIO::Get().SetAuthenticated(isAuth);
    }
}
