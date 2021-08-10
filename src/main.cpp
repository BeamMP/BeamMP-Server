#include "Common.h"
#include "Http.h"
#include "TSentry.h"
#include "TConfig.h"
#include "THeartbeatThread.h"
#include "TLuaEngine.h"
#include "TNetwork.h"
#include "TPPSMonitor.h"
#include "TResourceManager.h"
#include "TServer.h"

#include <sentry.h>
#include <thread>

#ifdef __unix
#include <csignal>

void UnixSignalHandler(int sig) {
    switch (sig) {
    case SIGPIPE:
        warn("ignoring SIGPIPE");
        break;
    case SIGTERM:
        info("gracefully shutting down via SIGTERM");
        Application::GracefullyShutdown();
        break;
    case SIGINT:
        info("gracefully shutting down via SIGINT");
        Application::GracefullyShutdown();
        break;
    default:
        debug("unhandled signal: " + std::to_string(sig));
        break;
    }
}
#endif // __unix

// this is provided by the build system, leave empty for source builds
// global, yes, this is ugly, no, it cant be done another way
TSentry Sentry { SECRET_SENTRY_URL };

int main(int argc, char** argv) try {
#ifdef __unix
#if DEBUG
    info("registering handlers for SIGINT, SIGTERM, SIGPIPE");
#endif // DEBUG
    signal(SIGPIPE, UnixSignalHandler);
    signal(SIGTERM, UnixSignalHandler);
#ifndef DEBUG
    signal(SIGINT, UnixSignalHandler);
#endif // DEBUG
#endif // __unix
    setlocale(LC_ALL, "C");

    bool Shutdown = false;
    Application::RegisterShutdownHandler([&Shutdown] { Shutdown = true; });

    TServer Server(argc, argv);
    TConfig Config;


    if (Config.Failed()) {
        info("Closing in 10 seconds");
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 1;
    }

    RegisterThread("Main");

    Sentry.PrintWelcome();
    TResourceManager ResourceManager;
    TPPSMonitor PPSMonitor(Server);
    THeartbeatThread Heartbeat(ResourceManager, Server);
    TNetwork Network(Server, PPSMonitor, ResourceManager);
    TLuaEngine LuaEngine(Server, Network);
    PPSMonitor.SetNetwork(Network);
    Application::Console().InitializeLuaConsole(LuaEngine);

    // TODO: replace
    while (!Shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
} catch (const std::exception& e) {
    error(e.what());
    Sentry.LogException(e, _file_basename, _line);
}
