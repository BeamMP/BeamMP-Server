#include "Common.h"

#include "TConsole.h"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <zlib.h>

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
