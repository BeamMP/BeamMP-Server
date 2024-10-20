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

#include "LuaAPI.h"
#include "Client.h"
#include "Common.h"
#include "CustomAssert.h"
#include "Settings.h"
#include "TLuaEngine.h"

#include <nlohmann/json.hpp>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

std::string LuaAPI::LuaToString(const sol::object Value, size_t Indent, bool QuoteStrings) {
    if (Indent > 80) {
        return "[[possible recursion, refusing to keep printing]]";
    }
    switch (Value.get_type()) {
    case sol::type::userdata: {
        std::stringstream ss;
        ss << "[[userdata: " << Value.as<sol::userdata>().pointer() << "]]";
        return ss.str();
    }
    case sol::type::thread: {
        std::stringstream ss;
        ss << "[[thread: " << Value.as<sol::thread>().pointer() << "]] {"
           << "\n";
        for (size_t i = 0; i < Indent; ++i) {
            ss << "\t";
        }
        ss << "status: " << std::to_string(int(Value.as<sol::thread>().status())) << "\n}";
        return ss.str();
    }
    case sol::type::lightuserdata: {
        std::stringstream ss;
        ss << "[[lightuserdata: " << Value.as<sol::lightuserdata>().pointer() << "]]";
        return ss.str();
    }
    case sol::type::string:
        if (QuoteStrings) {
            return "\"" + Value.as<std::string>() + "\"";
        } else {
            return Value.as<std::string>();
        }
    case sol::type::number: {
        std::stringstream ss;
        if (Value.is<int>()) {
            ss << Value.as<int>();
        } else {
            ss << Value.as<float>();
        }
        return ss.str();
    }
    case sol::type::lua_nil:
    case sol::type::none:
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
                Result << LuaToString(Entry.first, Indent + 1) << ": " << LuaToString(Entry.second, Indent + 1, true) << ",";
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
    case sol::type::poly:
        return "<poly>";
    default:
        return "<unprintable type>";
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
        ToPrint += LuaToString(static_cast<const sol::object>(Arg));
        ToPrint += "\t";
    }
    luaprint(ToPrint);
}

TEST_CASE("LuaAPI::MP::GetServerVersion") {
    const auto [ma, mi, pa] = LuaAPI::MP::GetServerVersion();
    const auto real = Application::ServerVersion();
    CHECK(ma == real.major);
    CHECK(mi == real.minor);
    CHECK(pa == real.patch);
}

static inline std::pair<bool, std::string> InternalTriggerClientEvent(int PlayerID, const std::string& EventName, const std::string& Data) {
    std::string Packet = "E:" + EventName + ":" + Data;
    if (PlayerID == -1) {
        LuaAPI::MP::Engine->Network().SendToAll(nullptr, StringToVector(Packet), true, true);
        return { true, "" };
    } else {
        auto MaybeClient = GetClient(LuaAPI::MP::Engine->Server(), PlayerID);
        if (!MaybeClient || MaybeClient.value().expired()) {
            beammp_lua_errorf("TriggerClientEvent invalid Player ID '{}'", PlayerID);
            return { false, "Invalid Player ID" };
        }
        auto c = MaybeClient.value().lock();
        if (!LuaAPI::MP::Engine->Network().Respond(*c, StringToVector(Packet), true)) {
            beammp_lua_errorf("Respond failed, dropping client {}", PlayerID);
            LuaAPI::MP::Engine->Network().ClientKick(*c, "Disconnected after failing to receive packets");
            return { false, "Respond failed, dropping client" };
        }
        return { true, "" };
    }
}

std::pair<bool, std::string> LuaAPI::MP::TriggerClientEvent(int PlayerID, const std::string& EventName, const sol::object& DataObj) {
    std::string Data = DataObj.as<std::string>();
    return InternalTriggerClientEvent(PlayerID, EventName, Data);
}

std::pair<bool, std::string> LuaAPI::MP::DropPlayer(int ID, std::optional<std::string> MaybeReason) {
    auto MaybeClient = GetClient(Engine->Server(), ID);
    if (!MaybeClient || MaybeClient.value().expired()) {
        beammp_lua_errorf("Tried to drop client with id {}, who doesn't exist", ID);
        return { false, "Player does not exist" };
    }
    auto c = MaybeClient.value().lock();
    LuaAPI::MP::Engine->Network().ClientKick(*c, MaybeReason.value_or("No reason"));
    return { true, "" };
}

std::pair<bool, std::string> LuaAPI::MP::SendChatMessage(int ID, const std::string& Message) {
    std::pair<bool, std::string> Result;
    std::string Packet = "C:Server: " + Message;
    if (ID == -1) {
        LogChatMessage("<Server> (to everyone) ", -1, Message);
        Engine->Network().SendToAll(nullptr, StringToVector(Packet), true, true);
        Result.first = true;
    } else {
        auto MaybeClient = GetClient(Engine->Server(), ID);
        if (MaybeClient && !MaybeClient.value().expired()) {
            auto c = MaybeClient.value().lock();
            if (!c->IsSynced()) {
                Result.first = false;
                Result.second = "Player still syncing data";
                return Result;
            }
            LogChatMessage("<Server> (to \"" + c->GetName() + "\")", -1, Message);
            if (!Engine->Network().Respond(*c, StringToVector(Packet), true)) {
                beammp_errorf("Failed to send chat message back to sender (id {}) - did the sender disconnect?", ID);
                // TODO: should we return an error here?
            }
            Result.first = true;
        } else {
            beammp_lua_error("SendChatMessage invalid argument [1] invalid ID");
            Result.first = false;
            Result.second = "Invalid Player ID";
        }
        return Result;
    }
    return Result;
}

std::pair<bool, std::string> LuaAPI::MP::SendNotification(int ID, const std::string& Message, const std::string& Icon, const std::string& Category) {
    std::pair<bool, std::string> Result;
    std::string Packet = "N" + Category + ":" + Icon + ":" + Message;
    if (ID == -1) {
        Engine->Network().SendToAll(nullptr, StringToVector(Packet), true, true);
        Result.first = true;
    } else {
        auto MaybeClient = GetClient(Engine->Server(), ID);
        if (MaybeClient) {
            auto c = MaybeClient.value().lock();
            if (!c->IsSynced()) {
                Result.first = false;
                Result.second = "Player is not synced yet";
                return Result;
            }
            if (!Engine->Network().Respond(*c, StringToVector(Packet), true)) {
                beammp_errorf("Failed to send notification to player (id {}) - did the player disconnect?", ID);
                Result.first = false;
                Result.second = "Failed to send packet";
            }
            Result.first = true;
        } else {
            beammp_lua_error("SendNotification invalid argument [1] invalid ID");
            Result.first = false;
            Result.second = "Invalid Player ID";
        }
        return Result;
    }
    return Result;
}

std::pair<bool, std::string> LuaAPI::MP::RemoveVehicle(int PID, int VID) {
    std::pair<bool, std::string> Result;
    auto MaybeClient = GetClient(Engine->Server(), PID);
    if (!MaybeClient || MaybeClient.value().expired()) {
        beammp_lua_error("RemoveVehicle invalid Player ID");
        Result.first = false;
        Result.second = "Invalid Player ID";
        return Result;
    }
    auto c = MaybeClient.value().lock();
    if (!c->GetCarData(VID).empty()) {
        std::string Destroy = "Od:" + std::to_string(PID) + "-" + std::to_string(VID);
        LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onVehicleDeleted", "", PID, VID));
        Engine->Network().SendToAll(nullptr, StringToVector(Destroy), true, true);
        c->DeleteCar(VID);
        Result.first = true;
    } else {
        Result.first = false;
        Result.second = "Vehicle does not exist";
    }
    return Result;
}

void LuaAPI::MP::Set(int ConfigID, sol::object NewValue) {
    switch (ConfigID) {
    case 0: // debug
        if (NewValue.is<bool>()) {
            Application::Settings.set(Settings::Key::General_Debug, NewValue.as<bool>());
            beammp_info(std::string("Set `Debug` to ") + (Application::Settings.getAsBool(Settings::Key::General_Debug) ? "true" : "false"));
        } else {
            beammp_lua_error("set invalid argument [2] expected boolean");
        }
        break;
    case 1: // private
        if (NewValue.is<bool>()) {
            Application::Settings.set(Settings::Key::General_Private, NewValue.as<bool>());
            beammp_info(std::string("Set `Private` to ") + (Application::Settings.getAsBool(Settings::Key::General_Private) ? "true" : "false"));
        } else {
            beammp_lua_error("set invalid argument [2] expected boolean");
        }
        break;
    case 2: // max cars
        if (NewValue.is<int>()) {
            Application::Settings.set(Settings::Key::General_MaxCars, NewValue.as<int>());
            beammp_info(std::string("Set `MaxCars` to ") + std::to_string(Application::Settings.getAsInt(Settings::Key::General_MaxCars)));
        } else {
            beammp_lua_error("set invalid argument [2] expected integer");
        }
        break;
    case 3: // max players
        if (NewValue.is<int>()) {
            Application::Settings.set(Settings::Key::General_MaxPlayers, NewValue.as<int>());
            beammp_info(std::string("Set `MaxPlayers` to ") + std::to_string(Application::Settings.getAsInt(Settings::Key::General_MaxPlayers)));
        } else {
            beammp_lua_error("set invalid argument [2] expected integer");
        }
        break;
    case 4: // Map
        if (NewValue.is<std::string>()) {
            Application::Settings.set(Settings::Key::General_Map, NewValue.as<std::string>());
            beammp_info(std::string("Set `Map` to ") + Application::Settings.getAsString(Settings::Key::General_Map));
        } else {
            beammp_lua_error("set invalid argument [2] expected string");
        }
        break;
    case 5: // Name
        if (NewValue.is<std::string>()) {
            Application::Settings.set(Settings::Key::General_Name, NewValue.as<std::string>());
            beammp_info(std::string("Set `Name` to ") + Application::Settings.getAsString(Settings::Key::General_Name));
        } else {
            beammp_lua_error("set invalid argument [2] expected string");
        }
        break;
    case 6: // Desc
        if (NewValue.is<std::string>()) {
            Application::Settings.set(Settings::Key::General_Description, NewValue.as<std::string>());
            beammp_info(std::string("Set `Description` to ") + Application::Settings.getAsString(Settings::Key::General_Description));
        } else {
            beammp_lua_error("set invalid argument [2] expected string");
        }
        break;
    case 7: // Information packet
        if (NewValue.is<bool>()) {
            Application::Settings.set(Settings::Key::General_InformationPacket, NewValue.as<bool>());
            beammp_info(std::string("Set `InformationPacket` to ") + (Application::Settings.getAsBool(Settings::Key::General_InformationPacket) ? "true" : "false"));
        } else {
            beammp_lua_error("set invalid argument [2] expected boolean");
        }
         break;
    default:
        beammp_warn("Invalid config ID \"" + std::to_string(ConfigID) + "\". Use `MP.Settings.*` enum for this.");
        break;
    }
}

TLuaValue LuaAPI::MP::Get(int ConfigID) {
    switch (ConfigID) {
    case 0: // debug
        return Application::Settings.getAsBool(Settings::Key::General_Debug);
    case 1: // private
        return Application::Settings.getAsBool(Settings::Key::General_Private);
    case 2: // max cars
        return Application::Settings.getAsInt(Settings::Key::General_MaxCars);
    case 3: // max players
        return Application::Settings.getAsInt(Settings::Key::General_MaxPlayers);
    case 4: // Map
        return Application::Settings.getAsString(Settings::Key::General_Map);
    case 5: // Name
        return Application::Settings.getAsString(Settings::Key::General_Name);
    case 6: // Desc
        return Application::Settings.getAsString(Settings::Key::General_Description);
    case 7: // Information packet
        return Application::Settings.getAsBool(Settings::Key::General_InformationPacket);
    default:
        beammp_warn("Invalid config ID \"" + std::to_string(ConfigID) + "\". Use `MP.Settings.*` enum for this.");
        return 0;
    }
}

void LuaAPI::MP::Sleep(size_t Ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(Ms));
}

bool LuaAPI::MP::IsPlayerConnected(int ID) {
    auto MaybeClient = GetClient(Engine->Server(), ID);
    if (MaybeClient && !MaybeClient.value().expired()) {
        return MaybeClient.value().lock()->IsUDPConnected();
    } else {
        return false;
    }
}

bool LuaAPI::MP::IsPlayerGuest(int ID) {
    auto MaybeClient = GetClient(Engine->Server(), ID);
    if (MaybeClient && !MaybeClient.value().expired()) {
        return MaybeClient.value().lock()->IsGuest();
    } else {
        return false;
    }
}

void LuaAPI::MP::PrintRaw(sol::variadic_args Args) {
    std::string ToPrint = "";
    for (const auto& Arg : Args) {
        ToPrint += LuaToString(static_cast<const sol::object>(Arg));
        ToPrint += "\t";
    }
#ifdef DOCTEST_CONFIG_DISABLE
    Application::Console().WriteRaw(ToPrint);
#endif
}

int LuaAPI::PanicHandler(lua_State* State) {
    beammp_lua_error("PANIC: " + sol::stack::get<std::string>(State, 1));
    return 0;
}

template <typename FnT, typename... ArgsT>
static std::pair<bool, std::string> FSWrapper(FnT Fn, ArgsT&&... Args) {
    std::error_code errc;
    std::pair<bool, std::string> Result;
    Fn(std::forward<ArgsT>(Args)..., errc);
    Result.first = errc == std::error_code {};
    if (!Result.first) {
        Result.second = errc.message();
    }
    return Result;
}

std::pair<bool, std::string> LuaAPI::FS::CreateDirectory(const std::string& Path) {
    std::error_code errc;
    std::pair<bool, std::string> Result;
    fs::create_directories(Path, errc);
    Result.first = errc == std::error_code {};
    if (!Result.first) {
        Result.second = errc.message();
    }
    return Result;
}

TEST_CASE("LuaAPI::FS::CreateDirectory") {
    std::string TestDir = "beammp_test_dir";
    fs::remove_all(TestDir);
    SUBCASE("Single level dir") {
        const auto [Ok, Err] = LuaAPI::FS::CreateDirectory(TestDir);
        CHECK(Ok);
        CHECK(Err == "");
        CHECK(fs::exists(TestDir));
    }
    SUBCASE("Multi level dir") {
        const auto [Ok, Err] = LuaAPI::FS::CreateDirectory(TestDir + "/a/b/c");
        CHECK(Ok);
        CHECK(Err == "");
        CHECK(fs::exists(TestDir + "/a/b/c"));
    }
    SUBCASE("Already exists") {
        const auto [Ok, Err] = LuaAPI::FS::CreateDirectory(TestDir);
        CHECK(Ok);
        CHECK(Err == "");
        CHECK(fs::exists(TestDir));
        const auto [Ok2, Err2] = LuaAPI::FS::CreateDirectory(TestDir);
        CHECK(Ok2);
        CHECK(Err2 == "");
    }
    fs::remove_all(TestDir);
}

std::pair<bool, std::string> LuaAPI::FS::Remove(const std::string& Path) {
    std::error_code errc;
    std::pair<bool, std::string> Result;
    fs::remove(fs::relative(Path), errc);
    Result.first = errc == std::error_code {};
    if (!Result.first) {
        Result.second = errc.message();
    }
    return Result;
}

TEST_CASE("LuaAPI::FS::Remove") {
    const std::string TestFileOrDir = "beammp_test_thing";
    SUBCASE("Remove existing directory") {
        fs::create_directory(TestFileOrDir);
        const auto [Ok, Err] = LuaAPI::FS::Remove(TestFileOrDir);
        CHECK(Ok);
        CHECK_EQ(Err, "");
        CHECK(!fs::exists(TestFileOrDir));
    }
    SUBCASE("Remove non-existing directory") {
        fs::remove_all(TestFileOrDir);
        const auto [Ok, Err] = LuaAPI::FS::Remove(TestFileOrDir);
        CHECK(Ok);
        CHECK_EQ(Err, "");
        CHECK(!fs::exists(TestFileOrDir));
    }
    // TODO: add tests for files
    // TODO: add tests for files and folders without access permissions (failure)
}

std::pair<bool, std::string> LuaAPI::FS::Rename(const std::string& Path, const std::string& NewPath) {
    std::error_code errc;
    std::pair<bool, std::string> Result;
    fs::rename(Path, NewPath, errc);
    Result.first = errc == std::error_code {};
    if (!Result.first) {
        Result.second = errc.message();
    }
    return Result;
}

TEST_CASE("LuaAPI::FS::Rename") {
    const auto TestDir = "beammp_test_dir";
    const auto OtherTestDir = "beammp_test_dir_2";
    fs::remove_all(OtherTestDir);
    fs::create_directory(TestDir);
    const auto [Ok, Err] = LuaAPI::FS::Rename(TestDir, OtherTestDir);
    CHECK(Ok);
    CHECK_EQ(Err, "");
    CHECK(!fs::exists(TestDir));
    CHECK(fs::exists(OtherTestDir));

    fs::remove_all(OtherTestDir);
    fs::remove_all(TestDir);
}

std::pair<bool, std::string> LuaAPI::FS::Copy(const std::string& Path, const std::string& NewPath) {
    std::error_code errc;
    std::pair<bool, std::string> Result;
    fs::copy(Path, NewPath, fs::copy_options::recursive, errc);
    Result.first = errc == std::error_code {};
    if (!Result.first) {
        Result.second = errc.message();
    }
    return Result;
}

TEST_CASE("LuaAPI::FS::Copy") {
    const auto TestDir = "beammp_test_dir";
    const auto OtherTestDir = "beammp_test_dir_2";
    fs::remove_all(OtherTestDir);
    fs::create_directory(TestDir);
    const auto [Ok, Err] = LuaAPI::FS::Copy(TestDir, OtherTestDir);
    CHECK(Ok);
    CHECK_EQ(Err, "");
    CHECK(fs::exists(TestDir));
    CHECK(fs::exists(OtherTestDir));

    fs::remove_all(OtherTestDir);
    fs::remove_all(TestDir);
}

bool LuaAPI::FS::Exists(const std::string& Path) {
    return fs::exists(Path);
}

TEST_CASE("LuaAPI::FS::Exists") {
    const auto TestDir = "beammp_test_dir";
    const auto OtherTestDir = "beammp_test_dir_2";
    fs::remove_all(OtherTestDir);
    fs::create_directory(TestDir);

    CHECK(LuaAPI::FS::Exists(TestDir));
    CHECK(!LuaAPI::FS::Exists(OtherTestDir));

    fs::remove_all(OtherTestDir);
    fs::remove_all(TestDir);
}

std::string LuaAPI::FS::GetFilename(const std::string& Path) {
    return fs::path(Path).filename().string();
}

TEST_CASE("LuaAPI::FS::GetFilename") {
    CHECK(LuaAPI::FS::GetFilename("test.txt") == "test.txt");
    CHECK(LuaAPI::FS::GetFilename("/test.txt") == "test.txt");
    CHECK(LuaAPI::FS::GetFilename("place/test.txt") == "test.txt");
    CHECK(LuaAPI::FS::GetFilename("/some/../place/test.txt") == "test.txt");
}

std::string LuaAPI::FS::GetExtension(const std::string& Path) {
    return fs::path(Path).extension().string();
}

TEST_CASE("LuaAPI::FS::GetExtension") {
    CHECK(LuaAPI::FS::GetExtension("test.txt") == ".txt");
    CHECK(LuaAPI::FS::GetExtension("/test.txt") == ".txt");
    CHECK(LuaAPI::FS::GetExtension("place/test.txt") == ".txt");
    CHECK(LuaAPI::FS::GetExtension("/some/../place/test.txt") == ".txt");
    CHECK(LuaAPI::FS::GetExtension("/some/../place/test") == "");
    CHECK(LuaAPI::FS::GetExtension("/some/../place/test.a.b.c") == ".c");
    CHECK(LuaAPI::FS::GetExtension("/some/../place/test.") == ".");
    CHECK(LuaAPI::FS::GetExtension("/some/../place/test.a.b.") == ".");
}

std::string LuaAPI::FS::GetParentFolder(const std::string& Path) {
    return fs::path(Path).parent_path().string();
}

TEST_CASE("LuaAPI::FS::GetParentFolder") {
    CHECK(LuaAPI::FS::GetParentFolder("test.txt") == "");
    CHECK(LuaAPI::FS::GetParentFolder("/test.txt") == "/");
    CHECK(LuaAPI::FS::GetParentFolder("place/test.txt") == "place");
    CHECK(LuaAPI::FS::GetParentFolder("/some/../place/test.txt") == "/some/../place");
}

// TODO: add tests
bool LuaAPI::FS::IsDirectory(const std::string& Path) {
    return fs::is_directory(Path);
}

// TODO: add tests
bool LuaAPI::FS::IsFile(const std::string& Path) {
    return fs::is_regular_file(Path);
}

// TODO: add tests
std::string LuaAPI::FS::ConcatPaths(sol::variadic_args Args) {
    fs::path Path;
    for (size_t i = 0; i < Args.size(); ++i) {
        auto Obj = Args[i];
        if (!Obj.is<std::string>()) {
            beammp_lua_error("FS.Concat called with non-string argument");
            return "";
        }
        Path += Obj.as<std::string>();
        if (i < Args.size() - 1 && !Path.empty()) {
            Path += fs::path::preferred_separator;
        }
    }
    auto Result = Path.lexically_normal().string();
    return Result;
}

static void JsonEncodeRecursive(nlohmann::json& json, const sol::object& left, const sol::object& right, bool is_array, size_t depth = 0) {
    if (depth > 100) {
        beammp_lua_error("json serialize will not go deeper than 100 nested tables, internal references assumed, aborted this path");
        return;
    }
    std::string key {};
    switch (left.get_type()) {
    case sol::type::lua_nil:
    case sol::type::none:
    case sol::type::poly:
    case sol::type::boolean:
    case sol::type::lightuserdata:
    case sol::type::userdata:
    case sol::type::thread:
    case sol::type::function:
    case sol::type::table:
        beammp_lua_error("JsonEncode: left side of table field is unexpected type");
        return;
    case sol::type::string:
        key = left.as<std::string>();
        break;
    case sol::type::number:
        if (left.is<int>()) {
            key = std::to_string(left.as<int>());
        } else {
            key = std::to_string(left.as<double>());
        }
        break;
    default:
        beammp_assert_not_reachable();
    }
    nlohmann::json value;
    switch (right.get_type()) {
    case sol::type::lua_nil:
    case sol::type::none:
        return;
    case sol::type::poly:
        beammp_lua_warn("unsure what to do with poly type in JsonEncode, ignoring");
        return;
    case sol::type::boolean:
        value = right.as<bool>();
        break;
    case sol::type::lightuserdata:
        beammp_lua_warn("unsure what to do with lightuserdata in JsonEncode, ignoring");
        return;
    case sol::type::userdata:
        beammp_lua_warn("unsure what to do with userdata in JsonEncode, ignoring");
        return;
    case sol::type::thread:
        beammp_lua_warn("unsure what to do with thread in JsonEncode, ignoring");
        return;
    case sol::type::string:
        value = right.as<std::string>();
        break;
    case sol::type::number: {
        if (right.is<int>()) {
            value = right.as<int>();
        } else {
            value = right.as<double>();
        }
        break;
    }
    case sol::type::function:
        beammp_lua_warn("unsure what to do with function in JsonEncode, ignoring");
        return;
    case sol::type::table: {
        if (right.as<sol::table>().empty()) {
            value = nlohmann::json::object();
        } else {
            bool local_is_array = true;
            for (const auto& pair : right.as<sol::table>()) {
                if (pair.first.get_type() != sol::type::number) {
                    local_is_array = false;
                }
            }
            for (const auto& pair : right.as<sol::table>()) {
                JsonEncodeRecursive(value, pair.first, pair.second, local_is_array, depth + 1);
            }
        }
        break;
    }
    default:
        beammp_assert_not_reachable();
    }
    if (is_array) {
        json.push_back(value);
    } else {
        json[key] = value;
    }
}

std::string LuaAPI::MP::JsonEncode(const sol::table& object) {
    nlohmann::json json;
    // table
    if (object.as<sol::table>().empty()) {
        json = nlohmann::json::object();
    } else {
        bool is_array = true;
        for (const auto& pair : object.as<sol::table>()) {
            if (pair.first.get_type() != sol::type::number) {
                is_array = false;
            }
        }
        for (const auto& entry : object) {
            JsonEncodeRecursive(json, entry.first, entry.second, is_array);
        }
    }
    return json.dump();
}

std::string LuaAPI::MP::JsonDiff(const std::string& a, const std::string& b) {
    if (!nlohmann::json::accept(a)) {
        beammp_lua_error("JsonDiff first argument is not valid json: `" + a + "`");
        return "";
    }
    if (!nlohmann::json::accept(b)) {
        beammp_lua_error("JsonDiff second argument is not valid json: `" + b + "`");
        return "";
    }
    auto a_json = nlohmann::json::parse(a);
    auto b_json = nlohmann::json::parse(b);
    return nlohmann::json::diff(a_json, b_json).dump();
}

std::string LuaAPI::MP::JsonDiffApply(const std::string& data, const std::string& patch) {
    if (!nlohmann::json::accept(data)) {
        beammp_lua_error("JsonDiffApply first argument is not valid json: `" + data + "`");
        return "";
    }
    if (!nlohmann::json::accept(patch)) {
        beammp_lua_error("JsonDiffApply second argument is not valid json: `" + patch + "`");
        return "";
    }
    auto a_json = nlohmann::json::parse(data);
    auto b_json = nlohmann::json::parse(patch);
    a_json.patch(b_json);
    return a_json.dump();
}

std::string LuaAPI::MP::JsonPrettify(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonPrettify argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).dump(4);
}

std::string LuaAPI::MP::JsonMinify(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonMinify argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).dump(-1);
}

std::string LuaAPI::MP::JsonFlatten(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonFlatten argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).flatten().dump(-1);
}

std::string LuaAPI::MP::JsonUnflatten(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonUnflatten argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).unflatten().dump(-1);
}

std::pair<bool, std::string> LuaAPI::MP::TriggerClientEventJson(int PlayerID, const std::string& EventName, const sol::table& Data) {
    return InternalTriggerClientEvent(PlayerID, EventName, JsonEncode(Data));
}
