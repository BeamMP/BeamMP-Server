#include "Common.h"

#include "TConsole.h"
#include <array>
#include <charconv>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <thread>
#include <zlib.h>

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
    info("please wait while all subsystems are shutting down...");
    std::unique_lock Lock(mShutdownHandlersMutex);
    for (auto& Handler : mShutdownHandlers) {
        Handler();
    }
}

std::array<int, 3> Application::VersionStrToInts(const std::string& str) {
    std::array<int, 3> Version;
    std::stringstream ss(str);
    for (int& i : Version) {
        std::string Part;
        std::getline(ss, Part, '.');
        std::from_chars(&*Part.begin(), &*Part.begin() + Part.size(), i);
    }
    return Version;
}

bool Application::IsOutdated(const std::array<int, 3>& Current, const std::array<int, 3>& Newest) {
    if (Newest[0] > Current[0]) {
        return true;
    } else if (Newest[0] == Current[0] && Newest[1] > Current[1]) {
        return true;
    } else if (Newest[0] == Current[0] && Newest[1] == Current[1] && Newest[2] > Current[2]) {
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
        auto MyVersion = VersionStrToInts(ServerVersion());
        auto RemoteVersion = VersionStrToInts(Response);
        if (IsOutdated(MyVersion, RemoteVersion)) {
            std::string RealVersionString = std::to_string(RemoteVersion[0]) + ".";
            RealVersionString += std::to_string(RemoteVersion[1]) + ".";
            RealVersionString += std::to_string(RemoteVersion[2]);
            warn(std::string(ANSI_YELLOW_BOLD) + "NEW VERSION OUT! There's a new version (v" + RealVersionString + ") of the BeamMP-Server available! For info on how to update your server, visit https://wiki.beammp.com/en/home/server-maintenance#updating-the-server." + std::string(ANSI_RESET));
        } else {
            info("Server up-to-date!");
        }
    } else {
        warn("Unable to fetch version from backend.");
        trace("got " + Response);
        auto Lock = Sentry.CreateExclusiveContext();
        Sentry.SetContext("get-response", { { "response", Response } });
        Sentry.LogError("failed to get server version", _file_basename, _line);
    }
}

std::string Comp(std::string Data) {
    std::array<char, Biggest> C {};
    // obsolete
    C.fill(0);
    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    defstream.avail_in = (uInt)Data.length();
    defstream.next_in = (Bytef*)&Data[0];
    defstream.avail_out = Biggest;
    defstream.next_out = reinterpret_cast<Bytef*>(C.data());
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_SYNC_FLUSH);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);
    size_t TO = defstream.total_out;
    std::string Ret(TO, 0);
    std::copy_n(C.begin(), TO, Ret.begin());
    return Ret;
}

std::string DeComp(std::string Compressed) {
    std::array<char, Biggest> C {};
    // not needed
    C.fill(0);
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = Biggest;
    infstream.next_in = (Bytef*)(&Compressed[0]);
    infstream.avail_out = Biggest;
    infstream.next_out = (Bytef*)(C.data());
    inflateInit(&infstream);
    inflate(&infstream, Z_SYNC_FLUSH);
    inflate(&infstream, Z_FINISH);
    inflateEnd(&infstream);
    size_t TO = infstream.total_out;
    std::string Ret(TO, 0);
    std::copy_n(C.begin(), TO, Ret.begin());
    return Ret;
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
    auto Lock = std::unique_lock(ThreadNameMapMutex);
    threadNameMap[std::this_thread::get_id()] = str;
}

Version::Version(uint8_t major, uint8_t minor, uint8_t patch)
    : major(major)
    , minor(minor)
    , patch(patch) { }

std::string Version::AsString() {
    std::stringstream ss {};
    ss << int(major) << "." << int(minor) << "." << int(patch);
    return ss.str();
void LogChatMessage(const std::string& name, int id, const std::string& msg) {
    std::stringstream ss;
    ss << "[CHAT] ";
    if (id != -1) {
        ss << "(" << id << ") <" << name << ">";
    } else {
        ss << name << "";
    }
    ss << msg;

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
        return std::to_string(GetLastError())
    }
#else // posix
    return std::strerror(errno);
#endif
}