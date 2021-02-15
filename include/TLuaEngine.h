#ifndef TLUAENGINE_H
#define TLUAENGINE_H

#include "Common.h"
#include "IThreaded.h"
#include <lua.hpp>
#include <memory>
#include <set>

class TLuaFile;

class TLuaEngine : public IThreaded {
public:
    using TSetOfLuaFile = std::set<std::unique_ptr<TLuaFile>>;

    TLuaEngine();

    virtual void operator()() override;

    const TSetOfLuaFile& LuaFiles() const { return _LuaFiles; }

    std::optional<std::reference_wrapper<TLuaFile>> GetScript(lua_State* L);

private:
    void FolderList(const std::string& Path, bool HotSwap);
    void RegisterFiles(const std::string& Path, bool HotSwap);
    bool NewFile(const std::string& Path);

    TSetOfLuaFile _LuaFiles;
};

#endif // TLUAENGINE_H
