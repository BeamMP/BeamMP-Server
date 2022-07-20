#include "TPluginMonitor.h"

#include "TLuaEngine.h"

TPluginMonitor::TPluginMonitor(const fs::path& Path, std::shared_ptr<TLuaEngine> Engine)
    : mEngine(Engine)
    , mPath(Path) {
    Application::SetSubsystemStatus("PluginMonitor", Application::Status::Starting);
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
        if (mThread.joinable()) {
            mThread.join();
        }
    });

    Start();
}

void TPluginMonitor::operator()() {
    RegisterThread("PluginMonitor");
    beammp_info("PluginMonitor started");
    Application::SetSubsystemStatus("PluginMonitor", Application::Status::Good);
    while (!Application::IsShuttingDown()) {
        std::vector<std::string> ToRemove;
        for (const auto& Pair : mFileTimes) {
            try {
                auto CurrentTime = fs::last_write_time(Pair.first);
                if (CurrentTime > Pair.second) {
                    mFileTimes[Pair.first] = CurrentTime;
                    // grandparent of the path should be Resources/Server
                    if (fs::equivalent(fs::path(Pair.first).parent_path().parent_path(), mPath)) {
                        beammp_infof("File \"{}\" changed, reloading", Pair.first);
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
                        mEngine->ReportErrors(mEngine->TriggerEvent("onFileChanged", "", Pair.first));
                    } else {
                        // is in subfolder, dont reload, just trigger an event
                        beammp_debugf("File \"{}\" changed, not reloading because it's in a subdirectory. Triggering 'onFileChanged' event instead", Pair.first);
                        mEngine->ReportErrors(mEngine->TriggerEvent("onFileChanged", "", Pair.first));
                    }
                }
            } catch (const std::exception& e) {
                ToRemove.push_back(Pair.first);
            }
            for (size_t i = 0; i < 3 && !Application::IsShuttingDown(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        for (const auto& File : ToRemove) {
            mFileTimes.erase(File);
            beammp_warnf("File \"{}\" couldn't be accessed, so it was removed from plugin hot reload monitor (probably got deleted)", File);
        }
    }
    Application::SetSubsystemStatus("PluginMonitor", Application::Status::Shutdown);
}
