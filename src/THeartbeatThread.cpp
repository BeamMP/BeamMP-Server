#include "THeartbeatThread.h"

#include "Client.h"
#include "Http.h"
//#include "SocketIO.h"
#include <sstream>

void THeartbeatThread::operator()() {
    std::string Body;
    std::string T;

    // these are "hot-change" related variables
    static std::string Last;

    static std::chrono::high_resolution_clock::time_point LastNormalUpdateTime = std::chrono::high_resolution_clock::now();
    bool isAuth = false;
    while (!mShutdown) {
        Body = GenerateCall();
        // a hot-change occurs when a setting has changed, to update the backend of that change.
        auto Now = std::chrono::high_resolution_clock::now();
        if (((Now - LastNormalUpdateTime) < std::chrono::seconds(5))
            || (Last == Body && (Now - LastNormalUpdateTime) < std::chrono::seconds(30))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Last = Body;
        LastNormalUpdateTime = Now;
        if (!Application::Settings.CustomIP.empty())
            Body += "&ip=" + Application::Settings.CustomIP;

        T = Http::POST("beammp.com", "/heartbeatv2", {}, Body, false);

        if (T.substr(0, 2) != "20") {
            //Backend system refused server startup!
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            T = Http::POST("beammp.com", "/heartbeatv2", {}, Body, false);
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

        //SocketIO::Get().SetAuthenticated(isAuth);
    }
}
std::string THeartbeatThread::GenerateCall() {
    std::stringstream Ret;

    Ret << "uuid=" << Application::Settings.Key
        << "&players=" << mServer.ClientCount()
        << "&maxplayers=" << Application::Settings.MaxPlayers
        << "&port=" << Application::Settings.Port
        << "&map=" << Application::Settings.MapName
        << "&private=" << (Application::Settings.Private ? "true" : "false")
        << "&version=" << Application::ServerVersion()
        << "&clientversion=" << Application::ClientVersion()
        << "&name=" << Application::Settings.ServerName
        << "&pps=" << Application::PPS()
        << "&modlist=" << mResourceManager.TrimmedList()
        << "&modstotalsize=" << mResourceManager.MaxModSize()
        << "&modstotal=" << mResourceManager.ModsLoaded()
        << "&playerslist=" << GetPlayers()
        << "&desc=" << Application::Settings.ServerDesc;
    return Ret.str();
}
THeartbeatThread::THeartbeatThread(TResourceManager& ResourceManager, TServer& Server)
    : mResourceManager(ResourceManager)
    , mServer(Server) {
    Application::RegisterShutdownHandler([&] {
        if (mThread.joinable()) {
            debug("shutting down Heartbeat");
            mShutdown = true;
            mThread.join();
            debug("shut down Heartbeat");
        }
    });
    Start();
}
std::string THeartbeatThread::GetPlayers() {
    std::string Return;
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        if (!ClientPtr.expired()) {
            Return += ClientPtr.lock()->GetName() + ";";
        }
        return true;
    });
    return Return;
}
/*THeartbeatThread::~THeartbeatThread() {
}*/
