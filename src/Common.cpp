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
    std::regex VersionRegex { R"(\d\.\d\.\d)" };
    auto Response = Http::GET("kortlepel.com", 443, "/v/s.html");
    bool Matches = std::regex_match(Response, VersionRegex);
    if (Matches) {
        auto MyVersion = VersionStrToInts(ServerVersion());
        auto RemoteVersion = VersionStrToInts(Response);
        if (IsOutdated(MyVersion, RemoteVersion)) {
            warn("NEW VERSION OUT! There's a new version (v" + Response + ") of the BeamMP-Server available! For info on how to update your server, visit https://wiki.beammp.com/en/home/server-maintenance#updating-the-server.");
        } else {
            info("Server up-to-date!");
        }
    } else {
        warn("Unable to fetch version from backend.");
#if DEBUG
        debug("got " + Response);
#endif // DEBUG
        Sentry.CreateExclusiveContext();
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

std::map<std::thread::id, std::string> threadNameMap;

std::string ThreadName(bool DebugModeOverride) {
    if (DebugModeOverride || Application::Settings.DebugModeEnabled) {
        auto id = std::this_thread::get_id();
        if (threadNameMap.find(id) != threadNameMap.end()) {
            // found
            return threadNameMap.at(id) + " ";
        }
    }
    return "";
}

void RegisterThread(const std::string str) {
    threadNameMap[std::this_thread::get_id()] = str;
}

void LogChatMessage(const std::string& name, int id, const std::string& msg) {
    std::stringstream ss;
    ss << "[CHAT] ";
    if (id != -1) {
        ss << "(" << id << ") <" << name << ">";
    } else {
        ss << name << "";
    }
    ss << msg;
    Application::Console().Write(ss.str());
}
