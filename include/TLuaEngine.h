#ifndef TLUAENGINE_H
#define TLUAENGINE_H

#include "Common.h"
#include "IThreaded.h"
#include "TServer.h"
#include <lua.hpp>
#include <memory>
#include <set>

class TLuaFile;

class TLuaEngine : public IThreaded {
public:
    explicit TLuaEngine(TServer& Server);

    using TSetOfLuaFile = std::set<std::unique_ptr<TLuaFile>>;

    void operator()() override;

    [[nodiscard]] const TSetOfLuaFile& LuaFiles() const { return mLuaFiles; }
    [[nodiscard]] TServer& Server() { return mServer; }
    [[nodiscard]] const TServer& Server() const { return mServer; }

    std::optional<std::reference_wrapper<TLuaFile>> GetScript(lua_State* L);

private:
    void FolderList(const std::string& Path, bool HotSwap);
    void RegisterFiles(const std::string& Path, bool HotSwap);
    bool NewFile(const std::string& Path);

    TServer& mServer;
    TSetOfLuaFile mLuaFiles;
};

#endif // TLUAENGINE_H
