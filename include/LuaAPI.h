#pragma once

#include "TLuaEngine.h"
#include <tuple>

namespace LuaAPI {
void Print(sol::variadic_args);
namespace MP {
    static inline TLuaEngine* Engine { nullptr };

    void GetOSName();
    std::tuple<int, int, int> GetServerVersion();
}
}
