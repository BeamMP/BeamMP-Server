#include "TSentry.h"

#include "ArgsParser.h"
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

static const std::string sCommandlineArguments = R"(
USAGE: 
    BeamMP-Server [arguments]
    
ARGUMENTS:
    --help              
                        Displays this help and exits.
    --config=/path/to/ServerConfig.toml
                        Absolute or relative path to the 
                        Server Config file, including the
                        filename. For paths and filenames with 
                        spaces, put quotes around the path.
    --version
                        Prints version info and exits.

EXAMPLES:
    BeamMP-Server --config=../MyWestCoastServerConfig.toml
        Runs the BeamMP-Server and uses the server config file 
        which is one directory above it and is named
        'MyWestCoastServerConfig.toml'.
)";

// this is provided by the build system, leave empty for source builds
// global, yes, this is ugly, no, it cant be done another way
TSentry Sentry {};

struct MainArguments {
    int argc {};
    char** argv {};
    std::vector<std::string_view> List;
    std::string InvokedAs;
};

int BeamMPServerMain(MainArguments Arguments);

int main(int argc, char** argv) {
    MainArguments Args { argc, argv, {}, argv[0] };
    Args.List.reserve(argc);
    for (int i = 1; i < argc; ++i) {
        Args.List.push_back(argv[i]);
    }
    int MainRet = 0;
    try {
        MainRet = BeamMPServerMain(std::move(Args));
    } catch (const std::exception& e) {
        beammp_error("A fatal exception has occurred and the server is forcefully shutting down.");
        beammp_error(e.what());
        Sentry.LogException(e, _file_basename, _line);
        MainRet = -1;
    }
    return MainRet;
}

int BeamMPServerMain(MainArguments Arguments) {
    setlocale(LC_ALL, "C");

    SetupSignalHandlers();

    ArgsParser Parser;
    Parser.RegisterArgument({ "help" }, ArgsParser::NONE);
    Parser.RegisterArgument({ "version" }, ArgsParser::NONE);
    Parser.RegisterArgument({ "config" }, ArgsParser::HAS_VALUE);
    Parser.Parse(Arguments.List);
    if (!Parser.Verify()) {
        return 1;
    }
    if (Parser.FoundArgument({ "help" })) {
        Application::Console().Internal().set_prompt("");
        Application::Console().WriteRaw(sCommandlineArguments);
        return 0;
    }
    if (Parser.FoundArgument({ "version" })) {
        Application::Console().Internal().set_prompt("");
        Application::Console().WriteRaw("BeamMP-Server v" + Application::ServerVersionString());
        return 0;
    }

    std::string ConfigPath = "ServerConfig.toml";
    if (Parser.FoundArgument({ "config" })) {
        auto MaybeConfigPath = Parser.GetValueOfArgument({ "config" });
        if (MaybeConfigPath.has_value()) {
            ConfigPath = MaybeConfigPath.value();
            beammp_info("Custom config requested via commandline: '" + ConfigPath + "'");
        }
    }

    bool Shutdown = false;
    Application::RegisterShutdownHandler([&Shutdown] { Shutdown = true; });
    Application::RegisterShutdownHandler([] {
        auto Futures = LuaAPI::MP::Engine->TriggerEvent("onShutdown", "");
        TLuaEngine::WaitForAll(Futures);
    });

    TServer Server(Arguments.List);
    TConfig Config(ConfigPath);
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

    Http::Server::SetupEnvironment();
    Http::Server::THttpServerInstance HttpServerInstance{};
    beammp_debug("cert.pem is " + std::to_string(fs::file_size("cert.pem")) + " bytes");
    beammp_debug("key.pem is " + std::to_string(fs::file_size("key.pem")) + " bytes");

    RegisterThread("Main(Waiting)");

    while (!Shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    beammp_info("Shutdown.");
    return 0;
}
