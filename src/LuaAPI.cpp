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
#include "Common.h"
#include "CustomAssert.h"
#include "TLuaEngine.h"
#include "Value.h"

#include <nlohmann/json.hpp>
#include <sol/types.hpp>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

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

TEST_CASE("LuaAPI::MP::GetServerVersion") {
    const auto [ma, mi, pa] = LuaAPI::MP::GetServerVersion();
    const auto real = Application::ServerVersion();
    CHECK(ma == real.major);
    CHECK(mi == real.minor);
    CHECK(pa == real.patch);
}

static inline std::pair<bool, std::string> InternalTriggerClientEvent(int PlayerID, const std::string& EventName, const std::string& Data) {
    throw std::runtime_error(fmt::format("NOT IMPLEMENTED: {}", __func__));
    /*
        std::string Packet = "E:" + EventName + ":" + Data;
        if (PlayerID == -1) {
            LuaAPI::MP::Engine->Network().SendToAll(nullptr, StringToVector(Packet), true, true);
            return { true, "" };
        } else {
            auto MaybeClient = GetClient(LuaAPI::MP::Engine->Server(), PlayerID);
            if (!MaybeClient) {
                beammp_lua_errorf("TriggerClientEvent invalid Player ID '{}'", PlayerID);
                return { false, "Invalid Player ID" };
            }
            auto c = MaybeClient.value();
            if (!LuaAPI::MP::Engine->Network().Respond(*c, StringToVector(Packet), true)) {
                beammp_lua_errorf("Respond failed, dropping client {}", PlayerID);
                LuaAPI::MP::Engine->Network().Disconnect(*c);
                return { false, "Respond failed, dropping client" };
            }
            return { true, "" };
        }*/
}

std::pair<bool, std::string> LuaAPI::MP::TriggerClientEvent(int PlayerID, const std::string& EventName, const sol::object& DataObj) {
    std::string Data = DataObj.as<std::string>();
    return InternalTriggerClientEvent(PlayerID, EventName, Data);
}

std::pair<bool, std::string> LuaAPI::MP::DropPlayer(int ID, std::optional<std::string> MaybeReason) {
    throw std::runtime_error(fmt::format("NOT IMPLEMENTED: {}", __func__));
    /*
    auto MaybeClient = GetClient(Engine->Server(), ID);
    if (!MaybeClient) {
        beammp_lua_errorf("Tried to drop client with id {}, who doesn't exist", ID);
        return { false, "Player does not exist" };
    }
    auto c = MaybeClient.value();
    LuaAPI::MP::Engine->Network().ClientKick(*c, MaybeReason.value_or("No reason"));
    return { true, "" };
    */
}

std::pair<bool, std::string> LuaAPI::MP::SendChatMessage(int ID, const std::string& Message) {
    throw std::runtime_error(fmt::format("NOT IMPLEMENTED: {}", __func__));
    /*
    std::pair<bool, std::string> Result;
    std::string Packet = "C:Server: " + Message;
    if (ID == -1) {
        LogChatMessage("<Server> (to everyone) ", -1, Message);
        Engine->Network().SendToAll(nullptr, StringToVector(Packet), true, true);
        Result.first = true;
    } else {
        auto MaybeClient = GetClient(Engine->Server(), ID);
        if (MaybeClient) {
            auto c = MaybeClient.value();
            if (!c->IsSynced) {
                Result.first = false;
                Result.second = "Player still syncing data";
                return Result;
            }
            LogChatMessage("<Server> (to \"" + c->Name.get() + "\")", -1, Message);
            if (!Engine->Network().Respond(*c, StringToVector(Packet), true)) {
                beammp_errorf("Failed to send chat message back to sender (id {}) - did the sender disconnect?", ID);
                beammp_infof("Disconnecting client {} for failure to receive a chat message (TCP disconnect)", c->Name.get());
                Engine->Network().Disconnect(c);
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
    */
}

std::pair<bool, std::string> LuaAPI::MP::RemoveVehicle(int PID, int VID) {
    throw std::runtime_error(fmt::format("NOT IMPLEMENTED: {}", __func__));
    /*
    std::pair<bool, std::string> Result;
    auto MaybeClient = GetClient(Engine->Server(), PID);
    if (!MaybeClient) {
        beammp_lua_error("RemoveVehicle invalid Player ID");
        Result.first = false;
        Result.second = "Invalid Player ID";
        return Result;
    }
    auto c = MaybeClient.value();
    if (!c->GetCarData(VID).empty()) {
        std::string Destroy = "Od:" + std::to_string(PID) + "-" + std::to_string(VID);
        Engine->Network().SendToAll(nullptr, StringToVector(Destroy), true, true);
        c->DeleteCar(VID);
        Result.first = true;
    } else {
        Result.first = false;
        Result.second = "Vehicle does not exist";
    }
    return Result;
    */
}

void LuaAPI::MP::Set(int ConfigID, sol::object NewValue) {
    switch (ConfigID) {
    case 0: // debug
        if (NewValue.is<bool>()) {
            Application::Settings.DebugModeEnabled = NewValue.as<bool>();
            beammp_info(std::string("Set `Debug` to ") + (Application::Settings.DebugModeEnabled ? "true" : "false"));
        } else {
            beammp_lua_error("set invalid argument [2] expected boolean");
        }
        break;
    case 1: // private
        if (NewValue.is<bool>()) {
            Application::Settings.Private = NewValue.as<bool>();
            beammp_info(std::string("Set `Private` to ") + (Application::Settings.Private ? "true" : "false"));
        } else {
            beammp_lua_error("set invalid argument [2] expected boolean");
        }
        break;
    case 2: // max cars
        if (NewValue.is<int>()) {
            Application::Settings.MaxCars = NewValue.as<int>();
            beammp_info(std::string("Set `MaxCars` to ") + std::to_string(Application::Settings.MaxCars));
        } else {
            beammp_lua_error("set invalid argument [2] expected integer");
        }
        break;
    case 3: // max players
        if (NewValue.is<int>()) {
            Application::Settings.MaxPlayers = NewValue.as<int>();
            beammp_info(std::string("Set `MaxPlayers` to ") + std::to_string(Application::Settings.MaxPlayers));
        } else {
            beammp_lua_error("set invalid argument [2] expected integer");
        }
        break;
    case 4: // Map
        if (NewValue.is<std::string>()) {
            Application::Settings.MapName = NewValue.as<std::string>();
            beammp_info(std::string("Set `Map` to ") + Application::Settings.MapName);
        } else {
            beammp_lua_error("set invalid argument [2] expected string");
        }
        break;
    case 5: // Name
        if (NewValue.is<std::string>()) {
            Application::Settings.ServerName = NewValue.as<std::string>();
            beammp_info(std::string("Set `Name` to ") + Application::Settings.ServerName);
        } else {
            beammp_lua_error("set invalid argument [2] expected string");
        }
        break;
    case 6: // Desc
        if (NewValue.is<std::string>()) {
            Application::Settings.ServerDesc = NewValue.as<std::string>();
            beammp_info(std::string("Set `Description` to ") + Application::Settings.ServerDesc);
        } else {
            beammp_lua_error("set invalid argument [2] expected string");
        }
        break;
    default:
        beammp_warn("Invalid config ID \"" + std::to_string(ConfigID) + "\". Use `MP.Settings.*` enum for this.");
        break;
    }
}

void LuaAPI::MP::Sleep(size_t Ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(Ms));
}

bool LuaAPI::MP::IsPlayerConnected(int ID) {
    throw std::runtime_error(fmt::format("NOT IMPLEMENTED: {}", __func__));
    /*
    auto MaybeClient = GetClient(Engine->Server(), ID);
    if (MaybeClient) {
        return MaybeClient.value()->IsConnected.get();
    } else {
        return false;
    }*/
}

bool LuaAPI::MP::IsPlayerGuest(int ID) {
    throw std::runtime_error(fmt::format("NOT IMPLEMENTED: {}", __func__));
    /*
    auto MaybeClient = GetClient(Engine->Server(), ID);
    if (MaybeClient) {
        return MaybeClient.value()->IsGuest.get();
    } else {
        return false;
    }
    */
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
        key = std::to_string(left.as<double>());
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
    case sol::type::number:
        value = right.as<double>();
        break;
    case sol::type::function:
        beammp_lua_warn("unsure what to do with function in JsonEncode, ignoring");
        return;
    case sol::type::table: {
        bool local_is_array = true;
        for (const auto& pair : right.as<sol::table>()) {
            if (pair.first.get_type() != sol::type::number) {
                local_is_array = false;
            }
        }
        for (const auto& pair : right.as<sol::table>()) {
            JsonEncodeRecursive(value, pair.first, pair.second, local_is_array, depth + 1);
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

static std::string lua_to_json_impl(const sol::object& args) {
    // used as the invalid value provider in sol_obj_to_value.
    auto special_stringifier = [](const sol::object& object) -> Result<Value> {
        beammp_lua_debugf("Cannot convert from type {} to json, ignoring (using null)", sol::to_string(object.get_type()));
        return { Null };
    };
    auto maybe_val = sol_obj_to_value(obj, special_stringifier);
    if (maybe_val) {
        auto result = boost::apply_visitor(ValueToJsonVisitor(ValueToStringVisitor::Flag::NONE), maybe_val.move());
        return result.dump();
    } else {
        beammp_lua_errorf("Failed to convert an argument to json: {}", maybe_val.error);
        return "";
    }
}

std::string LuaAPI::Util::JsonEncode(const sol::object& object) {
    return lua_to_json_impl(object);
}

std::string LuaAPI::Util::JsonDiff(const std::string& a, const std::string& b) {
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

std::string LuaAPI::Util::JsonDiffApply(const std::string& data, const std::string& patch) {
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

std::string LuaAPI::Util::JsonPrettify(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonPrettify argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).dump(4);
}

std::string LuaAPI::Util::JsonMinify(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonMinify argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).dump(-1);
}

std::string LuaAPI::Util::JsonFlatten(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonFlatten argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).flatten().dump(-1);
}

std::string LuaAPI::Util::JsonUnflatten(const std::string& json) {
    if (!nlohmann::json::accept(json)) {
        beammp_lua_error("JsonUnflatten argument is not valid json: `" + json + "`");
        return "";
    }
    return nlohmann::json::parse(json).unflatten().dump(-1);
}

std::pair<bool, std::string> LuaAPI::MP::TriggerClientEventJson(int PlayerID, const std::string& EventName, const sol::table& Data) {
    return InternalTriggerClientEvent(PlayerID, EventName, JsonEncode(Data));
}
size_t LuaAPI::MP::GetPlayerCount() { return Engine->Server().ClientCount(); }

static void JsonDecodeRecursive(sol::state_view& StateView, sol::table& table, const std::string& left, const nlohmann::json& right) {
    switch (right.type()) {
    case nlohmann::detail::value_t::null:
        return;
    case nlohmann::detail::value_t::object: {
        auto value = table.create();
        value.clear();
        for (const auto& entry : right.items()) {
            JsonDecodeRecursive(StateView, value, entry.key(), entry.value());
        }
        AddToTable(table, left, value);
        break;
    }
    case nlohmann::detail::value_t::array: {
        auto value = table.create();
        value.clear();
        for (const auto& entry : right.items()) {
            JsonDecodeRecursive(StateView, value, "", entry.value());
        }
        AddToTable(table, left, value);
        break;
    }
    case nlohmann::detail::value_t::string:
        AddToTable(table, left, right.get<std::string>());
        break;
    case nlohmann::detail::value_t::boolean:
        AddToTable(table, left, right.get<bool>());
        break;
    case nlohmann::detail::value_t::number_integer:
        AddToTable(table, left, right.get<int64_t>());
        break;
    case nlohmann::detail::value_t::number_unsigned:
        AddToTable(table, left, right.get<uint64_t>());
        break;
    case nlohmann::detail::value_t::number_float:
        AddToTable(table, left, right.get<double>());
        break;
    case nlohmann::detail::value_t::binary:
        beammp_lua_error("JsonDecode can't handle binary blob in json, ignoring");
        return;
    case nlohmann::detail::value_t::discarded:
        return;
    default:
        beammp_assert_not_reachable();
    }
}

sol::table LuaAPI::Util::JsonDecode(sol::this_state s, const std::string& string) {
    sol::state_view StateView(s);
    auto table = StateView.create_tab if (!nlohmann::json::accept(str)) {
        beammp_lua_error("string given to JsonDecode is not valid json: `" + str + "`");
        return sol::lua_nil;
    }
    nlohmann::json json = nlohmann::json::parse(str);
    if (json.is_object()) {
        for (const auto& entry : json.items()) {
            JsonDecodeRecursive(StateView, table, entry.key(), entry.value());
        }
    } else if (json.is_array()) {
        for (const auto& entry : json) {
            JsonDecodeRecursive(StateView, table, "", entry);
        }
    } else {
        beammp_lua_error("JsonDecode expected array or object json, instead got " + std::string(json.type_name()));
        return sol::lua_nil;
    }
    return table;
}

sol::table LuaAPI::FS::ListDirectories(sol::this_state s, const std::string& path) {
    if (!std::filesystem::exists(Path)) {
        return sol::lua_nil;
    }
    auto table = s.create_table();
    for (const auto& entry : std::filesystem::directory_iterator(Path)) {
        if (entry.is_directory()) {
            table[table.size() + 1] = entry.path().lexically_relative(Path).string();
        }
    }
    return table;
}

sol::table LuaAPI::FS::ListFiles(sol::this_state s, const std::string& path) {
    if (!std::filesystem::exists(Path)) {
        return sol::lua_nil;
    }
    auto table = s.create_table();
    for (const auto& entry : std::filesystem::directory_iterator(Path)) {
        if (entry.is_regular_file() || entry.is_symlink()) {
            table[table.size() + 1] = entry.path().lexically_relative(Path).string();
        }
    }
    return table;
}
