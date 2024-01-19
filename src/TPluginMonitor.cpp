// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "TPluginMonitor.h"

#include "TLuaEngine.h"
#include <filesystem>

TPluginMonitor::TPluginMonitor(const fs::path& Path, std::shared_ptr<TLuaEngine> Engine)
    : mEngine(Engine)
    , mPath(Path) {
    Application::SetSubsystemStatus("PluginMonitor", Application::Status::Starting);
    if (!fs::exists(mPath)) {
        fs::create_directories(mPath);
    }
    for (const auto& Entry : fs::recursive_directory_iterator(mPath, fs::directory_options::follow_directory_symlink)) {
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
                        Res->WaitUntilReady();
                        if (Res->Error) {
                            beammp_lua_errorf("Error while hot-reloading \"{}\": {}", Pair.first, Res->ErrorMessage);
                        } else {
                            mEngine->ReportErrors(mEngine->TriggerLocalEvent(StateID, "onInit"));
                            mEngine->ReportErrors(mEngine->TriggerEvent("onFileChanged", "", Pair.first));
                        }
                    } else {
                        // is in subfolder, dont reload, just trigger an event
                        beammp_debugf("File \"{}\" changed, not reloading because it's in a subdirectory. Triggering 'onFileChanged' event instead", Pair.first);
                        mEngine->ReportErrors(mEngine->TriggerEvent("onFileChanged", "", Pair.first));
                    }
                }
            } catch (const std::exception& e) {
                ToRemove.push_back(Pair.first);
            }
        }
        Application::SleepSafeSeconds(3);
        for (const auto& File : ToRemove) {
            mFileTimes.erase(File);
            beammp_warnf("File \"{}\" couldn't be accessed, so it was removed from plugin hot reload monitor (probably got deleted)", File);
        }
    }
    Application::SetSubsystemStatus("PluginMonitor", Application::Status::Shutdown);
}
