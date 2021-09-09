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
#include "SignalHandling.h"

#include <iostream>
#include <thread>

// this is provided by the build system, leave empty for source builds
// global, yes, this is ugly, no, it cant be done another way
TSentry Sentry {};

int main(int argc, char** argv) try {
    setlocale(LC_ALL, "C");

    SetupSignalHandlers();

    bool Shutdown = false;
    Application::RegisterShutdownHandler([&Shutdown] { Shutdown = true; });

    TServer Server(argc, argv);
    TConfig Config;

    if (Config.Failed()) {
        info("Closing in 10 seconds");
        // loop to make it possible to ctrl+c instead
        for (size_t i = 0; i < 20; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
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

