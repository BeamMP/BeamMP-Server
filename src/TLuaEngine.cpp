#include "TLuaEngine.h"
#include "CustomAssert.h"

TLuaEngine::TLuaEngine(TServer& Server, TNetwork& Network)
    : mNetwork(Network)
    , mServer(Server) {
    if (!fs::exists(Application::Settings.Resource)) {
        fs::create_directory(Application::Settings.Resource);
    }
    fs::path Path = fs::path(Application::Settings.Resource) / "Server";
    if (!fs::exists(Path)) {
        fs::create_directory(Path);
    }
    mResourceServerPath = Path;
    Application::RegisterShutdownHandler([&] {
        mShutdown = true;
        if (mThread.joinable()) {
            mThread.join();
        }
    });
}

void TLuaEngine::operator()() {
    RegisterThread("LuaEngine");
    // lua engine main thread
    CollectPlugins();
}

void TLuaEngine::CollectPlugins() {
    for (const auto& dir : fs::directory_iterator(mResourceServerPath)) {
        auto path = dir.path();
        path = fs::relative(path);
        if (!dir.is_directory()) {
            beammp_error("\"" + dir.path().string() + "\" is not a directory, skipping");
        } else {
            beammp_debug("found plugin directory: " + path.string());
        }
    }
}

void TLuaEngine::InitializePlugin(const fs::path& folder) {
    Assert(fs::exists(folder));
    Assert(fs::is_directory(folder));
}
