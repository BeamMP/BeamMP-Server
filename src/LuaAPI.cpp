#include "LuaAPI.h"
#include "Client.h"
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

bool LuaAPI::MP::TriggerClientEvent(int PlayerID, const std::string& EventName, const std::string& Data) {
    std::string Packet = "E:" + EventName + ":" + Data;
    if (PlayerID == -1)
        Engine->Network().SendToAll(nullptr, Packet, true, true);
    else {
        auto MaybeClient = GetClient(Engine->Server(), PlayerID);
        if (!MaybeClient || MaybeClient.value().expired()) {
            beammp_lua_error("TriggerClientEvent invalid Player ID");
            return false;
        }
        auto c = MaybeClient.value().lock();
        if (!Engine->Network().Respond(*c, Packet, true)) {
            beammp_lua_error("Respond failed");
            return false;
        }
    }
    return true;
}

void LuaAPI::MP::DropPlayer(int ID, std::optional<std::string> MaybeReason) {
    auto MaybeClient = GetClient(Engine->Server(), ID);
    if (!MaybeClient || MaybeClient.value().expired()) {
        return;
    }
    auto c = MaybeClient.value().lock();
    if (!Engine->Network().Respond(*c, "C:Server:You have been Kicked from the server! Reason: " + MaybeReason.value_or("No reason"), true)) {
        // Ignore
    }
    c->SetStatus(-2);
    beammp_info("Closing socket due to kick");
    CloseSocketProper(c->GetTCPSock());
}

void LuaAPI::MP::SendChatMessage(int ID, const std::string& Message) {
    std::string Packet = "C:Server: " + Message;
    if (ID == -1) {
        //LogChatMessage("<Server> (to everyone) ", -1, Message);
        Engine->Network().SendToAll(nullptr, Packet, true, true);
    } else {
        auto MaybeClient = GetClient(Engine->Server(), ID);
        if (MaybeClient && !MaybeClient.value().expired()) {
            auto c = MaybeClient.value().lock();
            if (!c->IsSynced())
                return;
            //LogChatMessage("<Server> (to \"" + c->GetName() + "\")", -1, msg);
            Engine->Network().Respond(*c, Packet, true);
        } else {
            beammp_lua_error("SendChatMessage invalid argument [1] invalid ID");
        }
    }
}

void LuaAPI::MP::RemoveVehicle(int PlayerID, int VehicleID) {
}

void LuaAPI::MP::Set(int ConfigID, sol::object NewValue) {
}

void LuaAPI::MP::Sleep(size_t Ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(Ms));
}

bool LuaAPI::MP::IsPlayerConnected(int ID) {
}

bool LuaAPI::MP::GetPlayerGuest(int ID) {
}
