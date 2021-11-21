#include "Common.h"

#include "TConsole.h"
#include <array>
#include <charconv>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <thread>

#include "CustomAssert.h"
#include "Http.h"

std::unique_ptr<TConsole> Application::mConsole = std::make_unique<TConsole>();

void Application::RegisterShutdownHandler(const TShutdownHandler& Handler) {
    std::unique_lock Lock(mShutdownHandlersMutex);
    if (Handler) {
        mShutdownHandlers.push_front(Handler);
    }
}

void Application::GracefullyShutdown() {
    static bool AlreadyShuttingDown = false;
    static uint8_t ShutdownAttempts = 0;
    if (AlreadyShuttingDown) {
        ++ShutdownAttempts;
        // hard shutdown at 2 additional tries
        if (ShutdownAttempts == 2) {
            beammp_info("hard shutdown forced by multiple shutdown requests");
            std::exit(0);
        }
        beammp_info("already shutting down!");
        return;
    } else {
        AlreadyShuttingDown = true;
    }
    beammp_trace("waiting for lock release");
    std::unique_lock Lock(mShutdownHandlersMutex);
    beammp_info("please wait while all subsystems are shutting down...");
    for (size_t i = 0; i < mShutdownHandlers.size(); ++i) {
        beammp_info("Subsystem " + std::to_string(i + 1) + "/" + std::to_string(mShutdownHandlers.size()) + " shutting down");
        mShutdownHandlers[i]();
    }
}

std::string Application::ServerVersionString() {
    return mVersion.AsString();
}

std::array<uint8_t, 3> Application::VersionStrToInts(const std::string& str) {
    std::array<uint8_t, 3> Version;
    std::stringstream ss(str);
    for (uint8_t& i : Version) {
        std::string Part;
        std::getline(ss, Part, '.');
        std::from_chars(&*Part.begin(), &*Part.begin() + Part.size(), i);
    }
    return Version;
}

// FIXME: This should be used by operator< on Version
bool Application::IsOutdated(const Version& Current, const Version& Newest) {
    if (Newest.major > Current.major) {
        return true;
    } else if (Newest.major == Current.major && Newest.minor > Current.minor) {
        return true;
    } else if (Newest.major == Current.major && Newest.minor == Current.minor && Newest.patch > Current.patch) {
        return true;
    } else {
        return false;
    }
}

void Application::CheckForUpdates() {
    // checks current version against latest version
    std::regex VersionRegex { R"(\d+\.\d+\.\d+\n*)" };
    auto Response = Http::GET(GetBackendHostname(), 443, "/v/s");
    bool Matches = std::regex_match(Response, VersionRegex);
    if (Matches) {
        auto MyVersion = ServerVersion();
        auto RemoteVersion = Version(VersionStrToInts(Response));
        if (IsOutdated(MyVersion, RemoteVersion)) {
            std::string RealVersionString = RemoteVersion.AsString();
            beammp_warn(std::string(ANSI_YELLOW_BOLD) + "NEW VERSION OUT! There's a new version (v" + RealVersionString + ") of the BeamMP-Server available! For more info visit https://wiki.beammp.com/en/home/server-maintenance#updating-the-server." + std::string(ANSI_RESET));
        } else {
            beammp_info("Server up-to-date!");
        }
    } else {
        beammp_warn("Unable to fetch version from backend.");
        beammp_trace("got " + Response);
        auto Lock = Sentry.CreateExclusiveContext();
        Sentry.SetContext("get-response", { { "response", Response } });
        Sentry.LogError("failed to get server version", _file_basename, _line);
    }
}

// thread name stuff

static std::map<std::thread::id, std::string> threadNameMap {};
static std::mutex ThreadNameMapMutex {};

std::string ThreadName(bool DebugModeOverride) {
    auto Lock = std::unique_lock(ThreadNameMapMutex);
    if (DebugModeOverride || Application::Settings.DebugModeEnabled) {
        auto id = std::this_thread::get_id();
        if (threadNameMap.find(id) != threadNameMap.end()) {
            // found
            return threadNameMap.at(id) + " ";
        }
    }
    return "";
}

void RegisterThread(const std::string& str) {
    std::string ThreadId;
#ifdef WIN32
    ThreadId = std::to_string(GetCurrentThreadId());
#else
    ThreadId = std::to_string(gettid());
#endif
    if (Application::Settings.DebugModeEnabled) {
        std::cout << ("Thread \"" + str + "\" is TID " + ThreadId) << std::endl;
    }
    auto Lock = std::unique_lock(ThreadNameMapMutex);
    threadNameMap[std::this_thread::get_id()] = str;
}

Version::Version(uint8_t major, uint8_t minor, uint8_t patch)
    : major(major)
    , minor(minor)
    , patch(patch) { }

Version::Version(const std::array<uint8_t, 3>& v)
    : Version(v[0], v[1], v[2]) {
}

std::string Version::AsString() {
    std::stringstream ss {};
    ss << int(major) << "." << int(minor) << "." << int(patch);
    return ss.str();
}

void LogChatMessage(const std::string& name, int id, const std::string& msg) {
    std::stringstream ss;
    ss << ThreadName();
    ss << "[CHAT] ";
    if (id != -1) {
        ss << "(" << id << ") <" << name << ">";
    } else {
        ss << name << "";
    }
    ss << msg;
    Application::Console().Write(ss.str());
}

std::string GetPlatformAgnosticErrorString() {
#ifdef WIN32
    // This will provide us with the error code and an error message, all in one.
    int err;
    char msgbuf[256];
    msgbuf[0] = '\0';

    err = GetLastError();

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgbuf,
        sizeof(msgbuf),
        nullptr);

    if (*msgbuf) {
        return std::to_string(GetLastError()) + " - " + std::string(msgbuf);
    } else {
        return std::to_string(GetLastError());
    }
#else // posix
    return std::strerror(errno);
#endif
}
