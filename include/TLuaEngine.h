#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "TLuaFile.h"
#include "TServer.h"
#include <lua.hpp>
#include <memory>
#include <optional>
#include <set>

#include <filesystem>
namespace fs = std::filesystem;

struct TLuaArgs final {
    std::vector<std::any> Args;
};

class TLuaEngine : public IThreaded {
public:
    explicit TLuaEngine(TServer& Server, TNetwork& Network);

    using TSetOfLuaFile = std::vector<std::shared_ptr<TLuaFile>>;

    void operator()() override;

    std::any TriggerLuaEvent(const std::string& Event, bool local, std::weak_ptr<TLuaFile> Caller, std::shared_ptr<TLuaArgs> arg, bool Wait);

    [[nodiscard]] TServer& Server() { return mServer; }
    [[nodiscard]] const TServer& Server() const { return mServer; }
    [[nodiscard]] TNetwork& Network() { return mNetwork; }
    [[nodiscard]] const TNetwork& Network() const { return mNetwork; }

    void UnregisterScript(std::shared_ptr<TLuaFile> Script);
    std::shared_ptr<TLuaFile> GetLuaFileOfState(lua_State* L);
    std::shared_ptr<TLuaFile> InsertNewLuaFile(const fs::path& FileName, const std::string& PluginName);
    void SendError(lua_State* L, const std::string& msg);

private:
    void FolderList(const std::string& Path, bool HotSwap);
    void RegisterFiles(const fs::path& Path, bool HotSwap);
    bool IsNewFile(const std::string& Path);

    TNetwork& mNetwork;
    TServer& mServer;
    std::string mPath;
    bool mShutdown { false };
    mutable std::mutex mLuaFilesMutex;
    TSetOfLuaFile mLuaFiles;
};
