#include "LuaAPI.h"
#include "TLuaEngine.h"

static std::string LuaToString(const sol::object& Value) {
    switch (Value.get_type()) {
    case sol::type::string:
        return Value.as<std::string>();
    case sol::type::number:
        return std::to_string(Value.as<float>());
    case sol::type::boolean:
        return Value.as<bool>() ? "true" : "false";
    case sol::type::table: {
        std::stringstream Result;
        auto Table = Value.as<sol::table>();
        Result << "[[table: " << Table.pointer() << "]]: {";
        if (!Table.empty()) {
            for (const auto& Entry : Table) {
                Result << "\n\t" << LuaToString(Entry.first) << ": " << LuaToString(Entry.second) << ",";
            }
            Result << "\n";
        }
        Result << "}";
        return Result.str();
    }
    case sol::type::function: {
        std::stringstream ss;
        ss << "[[function: " << Value.as<sol::function>().pointer() << "]]";
        return ss.str();
    }
    default:
        return "((unprintable type))";
    }
}

std::string LuaAPI::MP::GetOSName() {
#if WIN32
    return "Windows";
#elif __linux
    return "Linux";
#else
    return "Other";
#endif
}

std::tuple<int, int, int> LuaAPI::MP::GetServerVersion() {
    return { Application::ServerVersion().major, Application::ServerVersion().minor, Application::ServerVersion().patch };
}

void LuaAPI::Print(sol::variadic_args Args) {
    std::string ToPrint = "";
    for (const auto& Arg : Args) {
        ToPrint += LuaToString(Arg);
        ToPrint += "\t";
    }
    luaprint(ToPrint);
}
