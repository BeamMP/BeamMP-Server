#include "Value.h"
#include "boost/json/value_from.hpp"
#include "sol/as_args.hpp"
#include "sol/types.hpp"
#include <doctest/doctest.h>
#include <fmt/format.h>
#include <sol/sol.hpp>

ValueToStringVisitor::ValueToStringVisitor(Flag flags, int depth)
    : m_quote_strings(flags & QUOTE_STRINGS)
    , m_depth(depth) {
}

std::string ValueToStringVisitor::operator()(const std::string& str) const {
    if (m_quote_strings) {
        return fmt::format("\"{}\"", str);
    } else {
        return str;
    }
}

std::string ValueToStringVisitor::operator()(int64_t i) const {
    return fmt::format("{}", i);
}

std::string ValueToStringVisitor::operator()(double d) const {
    return fmt::format("{}", d);
}

std::string ValueToStringVisitor::operator()(Null) const {
    return "null";
}

std::string ValueToStringVisitor::operator()(Bool b) const {
    return b ? "true" : "false";
}

std::string ValueToStringVisitor::operator()(const ValueArray& array) const {
    std::string res = "[ ";
    size_t i = 0;
    for (const auto& elem : array) {
        res += fmt::format("\n{:>{}}{}", "", m_depth * 2, boost::apply_visitor(ValueToStringVisitor(QUOTE_STRINGS, m_depth + 1), elem));
        if (i + 2 <= array.size()) {
            res += ",";
        } else {
            res += "\n";
        }
        ++i;
    }
    return res += fmt::format("{:>{}}]", "", (m_depth == 0 ? 0 : (m_depth - 1) * 2));
}

std::string ValueToStringVisitor::operator()(const ValueTuple& array) const {
    std::string res = "( ";
    size_t i = 0;
    for (const auto& elem : array) {
        res += fmt::format("\n{:>{}}{}", "", m_depth * 2, boost::apply_visitor(ValueToStringVisitor(QUOTE_STRINGS, m_depth + 1), elem));
        if (i + 2 <= array.size()) {
            res += ",";
        } else {
            res += "\n";
        }
        ++i;
    }
    return res += fmt::format("{:>{}})", "", (m_depth == 0 ? 0 : (m_depth - 1) * 2));
}

std::string ValueToStringVisitor::operator()(const HashMap<std::string, Value>& map) const {
    std::string res = "{ ";
    size_t i = 0;
    for (const auto& [key, value] : map) {
        res += fmt::format("\n{:>{}}{}: {}", "", m_depth * 2, key, boost::apply_visitor(ValueToStringVisitor(QUOTE_STRINGS, m_depth + 1), value));
        if (i + 2 <= map.size()) {
            res += ",";
        } else {
            res += "\n";
        }
        ++i;
    }
    return res += fmt::format("{:>{}}}}", "", (m_depth == 0 ? 0 : (m_depth - 1) * 2));
}

TEST_CASE("Value constructors") {
    SUBCASE("string via const char*") {
        Value value = "hello, world";
        CHECK_EQ(value.which(), VALUE_TYPE_IDX_STRING);
    }
    SUBCASE("int via long literal") {
        Value value = int64_t(1l);
        CHECK_EQ(value.which(), VALUE_TYPE_IDX_INT);
    }
    SUBCASE("double via double literal") {
        Value value = 5.432;
        CHECK_EQ(value.which(), VALUE_TYPE_IDX_DOUBLE);
    }
    // other constructors must be explicit as far as we're concerned
}

TEST_CASE("ValueToStringVisitor") {
    SUBCASE("string quoted") {
        Value value = "hello, world";
        // expected to implicitly be "ValueToStringVisitor::Flag::QUOTE_STRINGS"
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "\"hello, world\"");
    }
    SUBCASE("string not quoted") {
        Value value = "hello, world";
        std::string res = boost::apply_visitor(ValueToStringVisitor(ValueToStringVisitor::Flag::NONE), value);
        CHECK_EQ(res, "hello, world");
    }
    SUBCASE("int") {
        Value value = int64_t(123456789l);
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "123456789");
    }
    SUBCASE("negative int") {
        Value value = int64_t(-123456789l);
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "-123456789");
    }
    SUBCASE("whole integer double") {
        Value value = 123456789.0;
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "123456789");
    }
    SUBCASE("double") {
        Value value = 1234.56789;
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "1234.56789");
    }
    SUBCASE("null") {
        Value value = Null {};
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "null");
    } /*
     SUBCASE("array") {
         Value value = ValueArray { 1l, 2l, "hello", 5.4 };
         std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
         CHECK_EQ(res, "[ 1, 2, \"hello\", 5.4 ]");
     }
     SUBCASE("tuple") {
         Value value = ValueArray { 1l, 2l, "hello" };
         std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
         CHECK_EQ(res, "( 1, 2, \"hello\" )");
     }
     SUBCASE("array with array inside") {
         Value value = ValueArray { 1l, 2l, "hello", ValueArray { -1l, -2l }, 5.4 };
         std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
         CHECK_EQ(res, "[ 1, 2, \"hello\", [ -1, -2 ], 5.4 ]");
     }
     SUBCASE("map") {
         Value value = ValueHashMap { { "hello", "world" }, { "x", 53.5 } };
         std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
         CHECK_EQ(res, R"({ hello="world", x=53.5 })");
     }
     SUBCASE("map with map inside") {
         Value value = ValueHashMap { { "hello", "world" }, { "my map", ValueHashMap { { "value", 1l } } }, { "x", 53.5 } };
         std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
         CHECK_EQ(res, R"({ hello="world", my map={ value=1 }, x=53.5 })");
     }
  */
    SUBCASE("empty array") {
        Value value = ValueArray {};
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "[ ]");
    }
    SUBCASE("empty tuple") {
        Value value = ValueTuple {};
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "( )");
    }
    SUBCASE("empty map") {
        Value value = ValueHashMap {};
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "{ }");
    }
    SUBCASE("bool") {
        Value value = Bool { false };
        std::string res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "false");

        value = Bool { true };
        res = boost::apply_visitor(ValueToStringVisitor(), value);
        CHECK_EQ(res, "true");
    }
}

Result<Value> sol_obj_to_value(const sol::object& obj, const std::function<Result<Value>(const sol::object&)>& invalid_provider) {
    switch (obj.get_type()) {
    case sol::type::none:
    case sol::type::lua_nil:
        return { Null {} };
    case sol::type::string:
        return { obj.as<std::string>() };
    case sol::type::number:
        if (obj.is<int64_t>()) {
            return { obj.as<int64_t>() };
        } else {
            return { obj.as<double>() };
        }
    case sol::type::thread: {
        if (invalid_provider) {
            return invalid_provider(obj);
        } else {
            return Result<Value>("Can't convert {} to Value", "lua thread");
        }
    }
    case sol::type::boolean:
        return { Bool { obj.as<bool>() } };
    case sol::type::function:
        if (invalid_provider) {
            return invalid_provider(obj);
        } else {
            return Result<Value>("Can't convert {} to Value", "lua function");
        }
    case sol::type::userdata:
        if (invalid_provider) {
            return invalid_provider(obj);
        } else {
            return Result<Value>("Can't convert {} to Value", "lua userdata");
        }
    case sol::type::lightuserdata:
        if (invalid_provider) {
            return invalid_provider(obj);
        } else {
            return Result<Value>("Can't convert {} to Value", "lua lightuserdata");
        }
    case sol::type::table: {
        bool is_map = false;
        // look through all keys, if all are numbers, its not a map
        for (const auto& [k, v] : obj.as<sol::table>()) {
            // due to a quirk in lua+sol (sol issue #247), you have to
            // both check that it's a number, but also that its an integer number.
            // technically, lua allows float keys in an array (i guess?)
            // but for us that counts as a type we need to use a hashmap for.
            // k.is<double>() would be true for any number type, but
            // k.is<int>() is only true for such numbers that are non-float.
            if (k.get_type() == sol::type::number) {
                if (!k.is<int64_t>()) {
                    is_map = true;
                    break;
                }
            } else if (k.get_type() == sol::type::string) {
                is_map = true;
                break;
            } else {
                return Result<Value>("Can't use non-string and non-number object as key for table (type id is {})", int(k.get_type())); // TODO: make a fucntion to fix htis messy way to enfore sending an error
            }
        }
        if (is_map) {
            ValueHashMap map;
            for (const auto& [k, v] : obj.as<sol::table>()) {
                std::string key;
                if (k.get_type() == sol::type::number) {
                    if (k.is<int64_t>()) {
                        key = fmt::format("{}", k.as<int64_t>());
                    } else {
                        key = fmt::format("{:F}", k.as<double>());
                    }
                } else if (k.get_type() == sol::type::string) {
                    key = k.as<std::string>();
                } else {
                    return Result<Value>("Failed to construct hash-map: Can't use non-string and non-number object as key for table{}", "");
                }
                auto maybe_val = sol_obj_to_value(v, invalid_provider);
                if (maybe_val) [[likely]] {
                    map.emplace(key, maybe_val.move());
                } else {
                    return maybe_val; // error
                }
            }
            return { std::move(map) };
        } else {
            ValueArray array;
            for (const auto& [k, v] : obj.as<sol::table>()) {
                auto i = k.as<int64_t>() - 1; // -1 due to lua arrays starting at 1
                if (size_t(i) >= array.size()) {
                    array.resize(size_t(i) + 1, Null {});
                }
                auto maybe_val = sol_obj_to_value(v, invalid_provider);
                if (maybe_val) [[likely]] {
                    array[size_t(i)] = maybe_val.move();
                } else {
                    return maybe_val; // error
                }
            }
            return { std::move(array) };
        }
    }
    case sol::type::poly:
        if (invalid_provider) {
            return invalid_provider(obj);
        } else {
            return Result<Value>("Can't convert {} to Value", "lua poly");
        }
    default:
        break;
    }
    return Result<Value>("Unknown type, can't convert to value.{}", "");
}

TEST_CASE("sol_obj_to_value") {
    sol::state state {};
    SUBCASE("nil") {
        sol::table obj = state.create_table();
        sol::object o = obj.get<sol::object>(0);
        CHECK_EQ(sol_obj_to_value(o).value().which(), VALUE_TYPE_IDX_NULL);
    }
    SUBCASE("string") {
        auto res = sol::make_object(state.lua_state(), "hello");
        CHECK_EQ(sol_obj_to_value(res).value().which(), VALUE_TYPE_IDX_STRING);
        auto val = sol_obj_to_value(res).value();
        auto str = boost::get<std::string>(val);
        CHECK_EQ(str, "hello");
    }
    SUBCASE("int") {
        auto res = sol::make_object(state.lua_state(), 1234l);
        CHECK_EQ(sol_obj_to_value(res).value().which(), VALUE_TYPE_IDX_INT);
        auto val = sol_obj_to_value(res).value();
        auto i = boost::get<int64_t>(val);
        CHECK_EQ(i, 1234l);
    }
    SUBCASE("double") {
        auto res = sol::make_object(state.lua_state(), 53.3);
        CHECK_EQ(sol_obj_to_value(res).value().which(), VALUE_TYPE_IDX_DOUBLE);
        auto val = sol_obj_to_value(res).value();
        auto d = boost::get<double>(val);
        CHECK_EQ(d, 53.3);
    }
    SUBCASE("bool") {
        auto res = sol::make_object(state.lua_state(), true);
        CHECK_EQ(sol_obj_to_value(res).value().which(), VALUE_TYPE_IDX_BOOL);
        auto val = sol_obj_to_value(res).value();
        auto d = boost::get<Bool>(val);
        CHECK_EQ(bool(d), true);
    }
    SUBCASE("table (map)") {
        auto res = state.create_table_with(
            "hello", "world",
            "x", 35l);
        CHECK_EQ(sol_obj_to_value(res).value().which(), VALUE_TYPE_IDX_HASHMAP);
        auto val = sol_obj_to_value(res).value();
        auto m = boost::get<ValueHashMap>(val);
        CHECK(m.contains("hello"));
        CHECK(m.contains("x"));
        CHECK_EQ(boost::get<std::string>(m["hello"]), "world");
        CHECK_EQ(boost::get<int64_t>(m["x"]), 35l);
    }
    SUBCASE("table (array)") {
        auto res = state.create_table_with(
            1, 1,
            2, 2,
            3, 3,
            6, 6);
        CHECK_EQ(sol_obj_to_value(res).value().which(), VALUE_TYPE_IDX_ARRAY);
        auto val = sol_obj_to_value(res).value();
        auto m = boost::get<ValueArray>(val);
        CHECK_EQ(boost::get<int64_t>(m[0]), 1);
        CHECK_EQ(boost::get<int64_t>(m[1]), 2);
        CHECK_EQ(boost::get<int64_t>(m[2]), 3);
        CHECK_EQ(m[3].which(), VALUE_TYPE_IDX_NULL);
        CHECK_EQ(m[4].which(), VALUE_TYPE_IDX_NULL);
        CHECK_EQ(boost::get<int64_t>(m[5]), 6);
    }
    // TODO: add test for all the invalid types
}

std::ostream& operator<<(std::ostream& os, const Null&) {
    return os << "null";
}

std::ostream& operator<<(std::ostream& os, const Bool& b) {
    return os << (b ? "true" : "false");
}

boost::json::value ValueToJsonVisitor::operator()(const std::string& str) const {
    return boost::json::value_from(str);
}

boost::json::value ValueToJsonVisitor::operator()(int64_t i) const {
    return boost::json::value_from(i);
}

boost::json::value ValueToJsonVisitor::operator()(double d) const {
    return boost::json::value_from(d);
}

boost::json::value ValueToJsonVisitor::operator()(Null) const {
    return boost::json::value_from(nullptr);
}

boost::json::value ValueToJsonVisitor::operator()(Bool b) const {
    return boost::json::value_from(b.b);
}

boost::json::value ValueToJsonVisitor::operator()(const ValueArray& array) const {
    auto ja = boost::json::array();
    for (const auto& val : array) {
        auto obj = boost::apply_visitor(ValueToJsonVisitor(), val);
        ja.push_back(obj);
    }
    return ja;
}

boost::json::value ValueToJsonVisitor::operator()(const ValueTuple& tuple) const {
    auto ja = boost::json::array();
    for (const auto& val : tuple) {
        auto obj = boost::apply_visitor(ValueToJsonVisitor(), val);
        ja.push_back(obj);
    }
    return ja;
}

boost::json::value ValueToJsonVisitor::operator()(const HashMap<std::string, Value>& map) const {
    auto jo = boost::json::object();
    for (const auto& [key, val] : map) {
        auto json_val = boost::apply_visitor(ValueToJsonVisitor(), val);
        jo.emplace(key, json_val);
    }
    return jo;
}

ValueToLuaVisitor::ValueToLuaVisitor(sol::state& state)
    : m_state(state) {
}

sol::object ValueToLuaVisitor::operator()(const std::string& str) const {
    return sol::make_object(m_state.lua_state(), str);
}

sol::object ValueToLuaVisitor::operator()(int64_t i) const {
    return sol::make_object(m_state.lua_state(), i);
}

sol::object ValueToLuaVisitor::operator()(double d) const {
    return sol::make_object(m_state.lua_state(), d);
}

sol::object ValueToLuaVisitor::operator()(Null) const {
    return sol::lua_nil_t();
}

sol::object ValueToLuaVisitor::operator()(Bool b) const {
    return sol::make_object(m_state.lua_state(), b.b);
}

sol::object ValueToLuaVisitor::operator()(const ValueArray& array) const {
    auto table = m_state.create_table();
    for (const auto& val : array) {
        table.add(boost::apply_visitor(*this, val));
    }
    return table;
}

sol::object ValueToLuaVisitor::operator()(const ValueTuple& tuple) const {
    auto table = m_state.create_table();
    for (const auto& val : tuple) {
        table.add(boost::apply_visitor(*this, val));
    }
    return table;
}

sol::object ValueToLuaVisitor::operator()(const HashMap<std::string, Value>& map) const {
    auto table = m_state.create_table();
    for (const auto& [key, val] : map) {
        table.set(key, boost::apply_visitor(*this, val));
    }
    return table;
}

