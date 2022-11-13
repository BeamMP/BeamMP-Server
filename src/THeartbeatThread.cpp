#include "THeartbeatThread.h"

#include "Client.h"
#include "Http.h"
//#include "SocketIO.h"
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <sstream>

namespace json = rapidjson;

void THeartbeatThread::operator()() {
    RegisterThread("Heartbeat");
    std::string Body;
    std::string T;

    // these are "hot-change" related variables
    static std::string Last;

    static TimeType::time_point LastNormalUpdateTime = TimeType::now();
    bool isAuth = false;
    size_t UpdateReminderCounter = 0;
    while (!Application::IsShuttingDown()) {
        ++UpdateReminderCounter;
        Body = GenerateCall();
        // a hot-change occurs when a setting has changed, to update the backend of that change.
        auto Now = TimeType::now();
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
        if (!Application::GetSettingString(StrCustomIP).empty()) {
            Body += "&ip=" + Application::GetSettingString(StrCustomIP);
        }

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

        json::Document Doc;
        bool Ok = false;
        for (const auto& Url : Application::GetBackendUrlsInOrder()) {
            T = Http::POST(Url, 443, Target, Body, "application/x-www-form-urlencoded", &ResponseCode, { { "api-v", "2" } });
            Doc.Parse(T.data(), T.size());
            if (Doc.HasParseError() || !Doc.IsObject()) {
                if (!Application::GetSettingBool(StrPrivate)) {
                    beammp_trace("Backend response failed to parse as valid json");
                    beammp_trace("Response was: `" + T + "`");
                }
                Sentry.SetContext("JSON Response", { { "reponse", T } });
                SentryReportError(Url + Target, ResponseCode);
            } else if (ResponseCode != 200) {
                SentryReportError(Url + Target, ResponseCode);
            } else {
                // all ok
                Ok = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::string Status {};
        std::string Code {};
        std::string Message {};
        const auto StatusKey = "status";
        const auto CodeKey = "code";
        const auto MessageKey = "msg";

        if (Ok) {
            if (Doc.HasMember(StatusKey) && Doc[StatusKey].IsString()) {
                Status = Doc[StatusKey].GetString();
            } else {
                Sentry.SetContext("JSON Response", { { StatusKey, "invalid string / missing" } });
                Ok = false;
            }
            if (Doc.HasMember(CodeKey) && Doc[CodeKey].IsString()) {
                Code = Doc[CodeKey].GetString();
            } else {
                Sentry.SetContext("JSON Response", { { CodeKey, "invalid string / missing" } });
                Ok = false;
            }
            if (Doc.HasMember(MessageKey) && Doc[MessageKey].IsString()) {
                Message = Doc[MessageKey].GetString();
            } else {
                Sentry.SetContext("JSON Response", { { MessageKey, "invalid string / missing" } });
                Ok = false;
            }
            if (!Ok) {
                beammp_error("Missing/invalid json members in backend response");
                Sentry.LogError("Missing/invalid json members in backend response", __FILE__, std::to_string(__LINE__));
            }
        } else {
            if (!Application::GetSettingBool(StrPrivate)) {
                beammp_warn("Backend failed to respond to a heartbeat. Your server may temporarily disappear from the server list. This is not an error, and will likely resolve itself soon. Direct connect will still work.");
            }
        }

        if (Ok && !isAuth && !Application::GetSettingBool(StrPrivate)) {
            if (Status == "2000") {
                beammp_info(("Authenticated! " + Message));
                isAuth = true;
            } else if (Status == "200") {
                beammp_info(("Resumed authenticated session! " + Message));
                isAuth = true;
            } else {
                if (Message.empty()) {
                    Message = "Backend didn't provide a reason.";
                }
                beammp_error("Backend REFUSED the auth key. Reason: " + Message);
            }
        }
        if (isAuth || Application::GetSettingBool(StrPrivate)) {
            Application::SetSubsystemStatus("Heartbeat", Application::Status::Good);
        }
        if (!Application::GetSettingBool(StrHideUpdateMessages) && UpdateReminderCounter % 5) {
            Application::CheckForUpdates();
        }
    }
}

std::string THeartbeatThread::GenerateCall() {
    std::stringstream Ret;

    Ret << "uuid=" << Application::GetSettingString(StrAuthKey)
        << "&players=" << mServer.ClientCount()
        << "&maxplayers=" << Application::GetSettingInt(StrMaxPlayers)
        << "&port=" << Application::GetSettingInt(StrPort)
        << "&map=" << Application::GetSettingString(StrMap)
        << "&private=" << (Application::GetSettingBool(StrPrivate) ? "true" : "false")
        << "&version=" << Application::ServerVersionString()
        << "&clientversion=" << std::to_string(Application::ClientMajorVersion()) + ".0" // FIXME: Wtf.
        << "&name=" << Application::GetSettingString(StrName)
        << "&modlist=" << mResourceManager.TrimmedList()
        << "&modstotalsize=" << mResourceManager.MaxModSize()
        << "&modstotal=" << mResourceManager.ModsLoaded()
        << "&playerslist=" << GetPlayers()
        << "&desc=" << Application::GetSettingString(StrDescription);
    return Ret.str();
}
THeartbeatThread::THeartbeatThread(TResourceManager& ResourceManager, TServer& Server)
    : mResourceManager(ResourceManager)
    , mServer(Server) {
    Application::SetSubsystemStatus("Heartbeat", Application::Status::Starting);
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("Heartbeat", Application::Status::ShuttingDown);
        if (mThread.joinable()) {
            mThread.join();
        }
        Application::SetSubsystemStatus("Heartbeat", Application::Status::Shutdown);
    });
    Start();
}
std::string THeartbeatThread::GetPlayers() {
    std::string Return;
    mServer.ForEachClient([&](const auto& Client) {
        Return += Client->GetName() + ";";
    });
    return Return;
}
/*THeartbeatThread::~THeartbeatThread() {
}*/
