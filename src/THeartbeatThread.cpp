#include "THeartbeatThread.h"

#include "Client.h"
#include "Http.h"
//#include "SocketIO.h"
#include <sstream>

void THeartbeatThread::operator()() {
    RegisterThread("Heartbeat");
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
        bool Unchanged = Last == Body;
        auto TimePassed = (Now - LastNormalUpdateTime);
        auto Threshold = Unchanged ? 30 : 5;
        if (TimePassed < std::chrono::seconds(Threshold)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        beammp_debug("heartbeat (after " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(TimePassed).count()) + "s)");

        Last = Body;
        LastNormalUpdateTime = Now;
        if (!Application::Settings.CustomIP.empty()) {
            Body += "&ip=" + Application::Settings.CustomIP;
        }

        Body += "&pps=" + Application::PPS();

        beammp_trace("heartbeat body: '" + Body + "'");

        auto SentryReportError = [&](const std::string& transaction, int status) {
            auto Lock = Sentry.CreateExclusiveContext();
            Sentry.SetContext("heartbeat",
                { { "response-body", T },
                    { "request-body", Body } });
            Sentry.SetTransaction(transaction);
            beammp_trace("sending log to sentry: " + std::to_string(status) + " for " + transaction);
            Sentry.Log(SentryLevel::Error, "default", Http::Status::ToString(status) + " (" + std::to_string(status) + ")");
        };

        auto Target = "/heartbeat";
        unsigned int ResponseCode = 0;
        T = Http::POST(Application::GetBackendHostname(), 443, Target, Body, "application/x-www-form-urlencoded", &ResponseCode);

        if ((T.substr(0, 2) != "20" && ResponseCode != 200) || ResponseCode != 200) {
            beammp_trace("got " + T + " from backend");
            Application::SetSubsystemStatus("Heartbeat", Application::Status::Bad);
            SentryReportError(Application::GetBackendHostname() + Target, ResponseCode);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            T = Http::POST(Application::GetBackup1Hostname(), 443, Target, Body, "application/x-www-form-urlencoded", &ResponseCode);
            if ((T.substr(0, 2) != "20" && ResponseCode != 200) || ResponseCode != 200) {
                SentryReportError(Application::GetBackup1Hostname() + Target, ResponseCode);
                Application::SetSubsystemStatus("Heartbeat", Application::Status::Bad);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                T = Http::POST(Application::GetBackup2Hostname(), 443, Target, Body, "application/x-www-form-urlencoded", &ResponseCode);
                if ((T.substr(0, 2) != "20" && ResponseCode != 200) || ResponseCode != 200) {
                    beammp_warn("Backend system refused server! Server will not show in the public server list.");
                    Application::SetSubsystemStatus("Heartbeat", Application::Status::Bad);
                    isAuth = false;
                    SentryReportError(Application::GetBackup2Hostname() + Target, ResponseCode);
                } else {
                    Application::SetSubsystemStatus("Heartbeat", Application::Status::Good);
                }
            } else {
                Application::SetSubsystemStatus("Heartbeat", Application::Status::Good);
            }
        } else {
            Application::SetSubsystemStatus("Heartbeat", Application::Status::Good);
        }

        if (!isAuth) {
            if (T == "2000") {
                beammp_info(("Authenticated!"));
                isAuth = true;
            } else if (T == "200") {
                beammp_info(("Resumed authenticated session!"));
                isAuth = true;
            }
        }

        // SocketIO::Get().SetAuthenticated(isAuth);
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
        << "&version=" << Application::ServerVersionString()
        << "&clientversion=" << Application::ClientVersionString()
        << "&name=" << Application::Settings.ServerName
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
    Application::SetSubsystemStatus("Heartbeat", Application::Status::Starting);
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("Heartbeat", Application::Status::ShuttingDown);
        if (mThread.joinable()) {
            mShutdown = true;
            mThread.join();
        }
        Application::SetSubsystemStatus("Heartbeat", Application::Status::Shutdown);
    });
    Start();
}
std::string THeartbeatThread::GetPlayers() {
    std::string Return;
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        ReadLock Lock(mServer.GetClientMutex());
        if (!ClientPtr.expired()) {
            Return += ClientPtr.lock()->GetName() + ";";
        }
        return true;
    });
    return Return;
}
/*THeartbeatThread::~THeartbeatThread() {
}*/
