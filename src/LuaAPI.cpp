#include "LuaAPI.h"
#include "TLuaEngine.h"

void LuaAPI::MP::GetOSName() {
}

std::tuple<int, int, int> LuaAPI::MP::GetServerVersion() {
    return { Application::ServerVersion().major, Application::ServerVersion().minor, Application::ServerVersion().patch };
}

void LuaAPI::Print(sol::variadic_args Args) {
    std::string ToPrint = "";
    for (const auto& Arg : Args) {
        if (Arg.get_type() == sol::type::string) {
            ToPrint += Arg.as<std::string>();
        } else {
            ToPrint += "((unprintable type))";
        }
        ToPrint += " ";
    }
    luaprint(ToPrint);
}
