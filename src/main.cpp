#include "TSentry.h"

#include "Common.h"
#include "CustomAssert.h"
#include "Http.h"
#include "TConfig.h"
#include "THeartbeatThread.h"
#include "TLuaEngine.h"
#include "TNetwork.h"
#include "TPPSMonitor.h"
#include "TResourceManager.h"
#include "TServer.h"

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
TSentry Sentry {};

#include <iostream>

int main(int argc, char** argv) try {
#ifdef __unix
    trace("registering handlers for SIGINT, SIGTERM, SIGPIPE");
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

    trace("Running in debug mode on a debug build");

    Sentry.SetupUser();
    Sentry.PrintWelcome();
    Application::CheckForUpdates();
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
