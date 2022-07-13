#include "TPluginMonitor.h"

#include "TLuaEngine.h"

TPluginMonitor::TPluginMonitor(const fs::path& Path, std::shared_ptr<TLuaEngine> Engine)
    : mEngine(Engine)
    , mPath(Path) {
    if (!fs::exists(mPath)) {
        fs::create_directories(mPath);
    }
    for (const auto& Entry : fs::recursive_directory_iterator(mPath)) {
        // TODO: trigger an event when a subfolder file changes
        if (Entry.is_regular_file()) {
            mFileTimes[Entry.path().string()] = fs::last_write_time(Entry.path());
        }
    }

    Application::RegisterShutdownHandler([this] {
        mShutdown = true;
        if (mThread.joinable()) {
            mThread.join();
        }
    });

    Start();
}

void TPluginMonitor::operator()() {
    RegisterThread("PluginMonitor");
    beammp_info("PluginMonitor started");
    while (!mShutdown) {
        std::vector<std::string> ToRemove;
        for (const auto& Pair : mFileTimes) {
            try {
                auto CurrentTime = fs::last_write_time(Pair.first);
                if (CurrentTime != Pair.second) {
                    mFileTimes[Pair.first] = CurrentTime;
                    // grandparent of the path should be Resources/Server
                    if (fs::equivalent(fs::path(Pair.first).parent_path().parent_path(), mPath)) {
                        beammp_info("File \"" + Pair.first + "\" changed, reloading");
                        // is in root folder, so reload
                        std::ifstream FileStream(Pair.first, std::ios::in | std::ios::binary);
                        auto Size = std::filesystem::file_size(Pair.first);
                        auto Contents = std::make_shared<std::string>();
                        Contents->resize(Size);
                        FileStream.read(Contents->data(), Contents->size());
                        TLuaChunk Chunk(Contents, Pair.first, fs::path(Pair.first).parent_path().string());
                        auto StateID = mEngine->GetStateIDForPlugin(fs::path(Pair.first).parent_path());
                        auto Res = mEngine->EnqueueScript(StateID, Chunk);
                        mEngine->AddResultToCheck(Res);
                        mEngine->ReportErrors(mEngine->TriggerLocalEvent(StateID, "onInit"));
                    } else {
                        // TODO: trigger onFileChanged event
                        beammp_trace("Change detected in file \"" + Pair.first + "\", event trigger not implemented yet");
                        /*
                        // is in subfolder, dont reload, just trigger an event
                        auto Results = mEngine.TriggerEvent("onFileChanged", "", Pair.first);
                        mEngine.WaitForAll(Results);
                        for (const auto& Result : Results)  {
                            if (Result->Error) {
                                beammp_lua_error(Result->ErrorMessage);
                            }
                        }*/
                    }
                }
            } catch (const std::exception& e) {
                ToRemove.push_back(Pair.first);
            }
            for (size_t i = 0; i < 3 && !mShutdown; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(i));
            }
        }
        for (const auto& File : ToRemove) {
            mFileTimes.erase(File);
            beammp_warn("file '" + File + "' couldn't be accessed, so it was removed from plugin hot reload monitor (probably got deleted)");
        }
    }
}
