#include "TSentry.h"

#include "Common.h"
#include "CustomAssert.h"
#include "Http.h"
#include "LuaAPI.h"
#include "SignalHandling.h"
#include "TConfig.h"
#include "THeartbeatThread.h"
#include "TLuaEngine.h"
#include "TNetwork.h"
#include "TPPSMonitor.h"
#include "TResourceManager.h"
#include "TScopedTimer.h"
#include "TServer.h"

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
    Application::RegisterShutdownHandler([] {
        auto Futures = LuaAPI::MP::Engine->TriggerEvent("onShutdown", "");
        TLuaEngine::WaitForAll(Futures);
    });

    TServer Server(argc, argv);
    TConfig Config;
    TLuaEngine LuaEngine;
    LuaEngine.SetServer(&Server);

    if (Config.Failed()) {
        beammp_info("Closing in 10 seconds");
        // loop to make it possible to ctrl+c instead
        for (size_t i = 0; i < 20; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        return 1;
    }

    RegisterThread("Main");

    beammp_trace("Running in debug mode on a debug build");
    Sentry.SetupUser();
    Sentry.PrintWelcome();
    TResourceManager ResourceManager;
    TPPSMonitor PPSMonitor(Server);
    THeartbeatThread Heartbeat(ResourceManager, Server);
    TNetwork Network(Server, PPSMonitor, ResourceManager);
    LuaEngine.SetNetwork(&Network);
    PPSMonitor.SetNetwork(Network);
    Application::Console().InitializeLuaConsole(LuaEngine);
    Application::CheckForUpdates();

    // TODO: replace
    while (!Shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    beammp_info("Shutdown.");
    return 0;
} catch (const std::exception& e) {
    beammp_error(e.what());
    Sentry.LogException(e, _file_basename, _line);
}
