#include "LuaAPI.h"
#include "TLuaEngine.h"

static std::string LuaToString(const sol::object& Value, size_t Indent = 1) {
    switch (Value.get_type()) {
    case sol::type::string:
        return Value.as<std::string>();
    case sol::type::number:
        return std::to_string(Value.as<float>());
    case sol::type::nil:
        return "<nil>";
    case sol::type::boolean:
        return Value.as<bool>() ? "true" : "false";
    case sol::type::table: {
        std::stringstream Result;
        auto Table = Value.as<sol::table>();
        Result << "[[table: " << Table.pointer() << "]]: {";
        if (!Table.empty()) {
            for (const auto& Entry : Table) {
                Result << "\n";
                for (size_t i = 0; i < Indent; ++i) {
                    Result << "\t";
                }
                Result << LuaToString(Entry.first, Indent + 1) << ": " << LuaToString(Entry.second, Indent + 1) << ",";
            }
            Result << "\n";
        }
        for (size_t i = 0; i < Indent - 1; ++i) {
            Result << "\t";
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
