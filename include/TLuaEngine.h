#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "TLuaFile.h"
#include "TServer.h"
#include <optional>
#include <lua.hpp>
#include <memory>
#include <set>
#include <unordered_map>

class TLuaEngine : public IThreaded {
public:
    explicit TLuaEngine(TServer& Server, TNetwork& Network);

    using TSetOfLuaFile = std::set<std::unique_ptr<TLuaFile>>;

    void operator()() override;

    [[nodiscard]] const TSetOfLuaFile& LuaFiles() const { return mLuaFiles; }
    [[nodiscard]] TServer& Server() { return mServer; }
    [[nodiscard]] const TServer& Server() const { return mServer; }
    [[nodiscard]] TNetwork& Network() { return mNetwork; }
    [[nodiscard]] const TNetwork& Network() const { return mNetwork; }

    std::optional<std::reference_wrapper<TLuaFile>> GetScript(lua_State* L);

    static std::unordered_map<std::string, lua_State*> mGlobals;
private:
    void FolderList(const std::string& Path, bool HotSwap);
    void RegisterFiles(const fs::path& Path, bool HotSwap);
    bool IsNewFile(const std::string& Path);

    TNetwork& mNetwork;
    TServer& mServer;
    std::string mPath;
    bool mShutdown { false };
    TSetOfLuaFile mLuaFiles;
    std::mutex mListMutex;
};
