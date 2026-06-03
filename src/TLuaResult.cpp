// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2026 BeamMP Ltd., BeamMP team and contributors.
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

#include "TLuaResult.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <sol/unsafe_function_result.hpp>
#include <stdexcept>
#include <thread>


std::ostream& operator<<(std::ostream& os, const TDetachedLuaValue& value) {
    std::visit([&os](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, TDetachedLuaValue::Array>) {
            size_t i = 0;
            for (const auto& val : arg) {
                if (i > 0) {
                    os << ", ";
                }
                os << val;
                ++i;
            }
        } else if constexpr (std::is_same_v<T, TDetachedLuaValue::Object>) {
            size_t i = 0;
            for (const auto& [key, val] : arg) {
                if (i > 0) {
                    os << ", ";
                }
                os << key << "=" << val;
                ++i;
            }
        } else if constexpr (std::is_same_v<T, bool>)
            os << (arg ? "true" : "false");
        else if constexpr (std::is_same_v<T, double>)
            os << arg;
        else if constexpr (std::is_same_v<T, int>)
            os << arg;
        else if constexpr (std::is_same_v<T, std::string>)
            os << arg;
        else if constexpr (std::is_same_v<T, std::monostate>)
            // monostate means no result value
            os << "";
        else
            static_assert(AlwaysFalseV<T>, "non-exhaustive visitor!");
    },
        value.V);

    return os;
}

void TLuaVoidResult::MarkReadySuccess() {
    std::unique_lock Lock(mMutex);
    mError = false;
    mErrorMessage.clear();
    MarkAsReady();
}

void TLuaVoidResult::MarkReadyError(sol::protected_function_result Res) {
    std::unique_lock Lock(mMutex);
    mError = true;
    SetErrorMessageFromResult(Res);
    MarkAsReady();
}

void TLuaVoidResult::MarkReadyError(std::string Res) {
    std::unique_lock Lock(mMutex);
    mError = true;
    mErrorMessage = std::move(Res);
    MarkAsReady();
}

bool TLuaVoidResult::IsReady() const {
    std::unique_lock Lock(mMutex);
    return mReady;
}

bool TLuaVoidResult::IsError() const {
    std::unique_lock Lock(mMutex);
    return mError;
}

TLuaVoidResult::Snapshot TLuaVoidResult::GetSnapshot() const {
    std::unique_lock Lock(mMutex);
    Snapshot snapshot {
        .Error = mError,
        .Ready = mReady,
        .ErrorMessage = mErrorMessage,
        .StateId = mStateId,
    };
    return snapshot;
}

void TLuaVoidResult::MarkAsReady() {
    mReady = true;
    mReadyCondition.notify_all();
}

void TLuaVoidResult::WaitUntilReady() {
    std::unique_lock Lock(mMutex);
    while (!mReady)
        mReadyCondition.wait_for(Lock, std::chrono::milliseconds(50),
            [this] {
                return mReady;
            });
}


void TLuaResult::MarkReadySuccess(sol::object Res) {
    std::unique_lock Lock(mMutex);
    mError = false;
    mDetachedResult = Freeze(Res);

    MarkAsReady();
}

void TLuaResult::MarkReadySuccessNoResult() {
    std::unique_lock Lock(mMutex);
    mError = false;
    mDetachedResult = { { std::monostate { } } };

    MarkAsReady();
}

void TLuaResult::MarkReadyError(sol::protected_function_result Res) {
    std::unique_lock Lock(mMutex);
    mError = true;
    SetErrorMessageFromResult(Res);

    MarkAsReady();
}

void TLuaResult::MarkReadyError(std::string Res) {
    std::unique_lock Lock(mMutex);
    mError = true;
    mErrorMessage = std::move(Res);

    MarkAsReady();
}

bool TLuaResult::IsReady() const {
    std::unique_lock Lock(mMutex);
    return mReady;
}
bool TLuaResult::IsError() const {
    std::unique_lock Lock(mMutex);
    return mError;
}

TLuaStateId TLuaResult::OwnerState() const {
    std::unique_lock Lock(mMutex);
    // copy
    return mStateId;
}
void TLuaResult::SetOwnerState(TLuaStateId StateId) {
    std::unique_lock Lock(mMutex);
    mStateId = std::move(StateId);
}

TLuaResult::DetachedSnapshot TLuaResult::GetDetachedSnapshot() const {
    std::unique_lock Lock(mMutex);
    DetachedSnapshot snapshot {
        .Error = mError,
        .Ready = mReady,
        .ErrorMessage = mErrorMessage,
        .Result = mDetachedResult,
        .StateId = mStateId,
        .Function = mFunction,
    };
    return snapshot;
}

TDetachedLuaValue TLuaResult::Freeze(const sol::object& o, int depth) {
    if (depth > 64)
        throw std::runtime_error("max depth (64) reached");
    switch (o.get_type()) {
    case sol::type::lua_nil:
        return { { std::monostate { } } };
    case sol::type::boolean:
        return { { o.as<bool>() } };
    case sol::type::number: {
        if (o.is<int>()) {
            return { { o.as<int>() } };
        } else {
            return { { o.as<double>() } };
        }
    }
    case sol::type::string:
        return { { o.as<std::string>() } };
    case sol::type::table: {
        TDetachedLuaValue::Object ObjectOut;
        // numeric stuff is for arrays
        std::vector<std::pair<size_t, TDetachedLuaValue>> NumericEntries;
        bool HasNonStringOrNumericKey = false;
        size_t MaxNumericIndex = 0;
        for (auto&& [k, v] : o.as<sol::table>()) {
            if (k.is<std::string>()) {
                ObjectOut.emplace(k.as<std::string>(), std::make_shared<TDetachedLuaValue>(std::move(Freeze(v, depth + 1))));
                continue;
            }
            // Thanks to lua handling arrays in a weird way, and because they can also be sparse, this weird code is needed.
            if (k.get_type() == sol::type::number) {
                const double NumericKey = k.as<double>();
                const size_t CandidateIndex = static_cast<size_t>(NumericKey);
                // unsure if we need to do this, or if we can do the same int check we do in other places, but this works well
                const bool IsPositiveInteger = std::isfinite(NumericKey)
                    && NumericKey >= 1.0
                    && std::fabs(NumericKey - static_cast<double>(CandidateIndex)) <= std::numeric_limits<double>::epsilon();
                if (IsPositiveInteger) {
                    const auto Index = CandidateIndex;
                    MaxNumericIndex = std::max(MaxNumericIndex, Index);
                    NumericEntries.emplace_back(Index, Freeze(v, depth + 1));
                    continue;
                }
            }
            HasNonStringOrNumericKey = true;
        }
        // Pure numeric-keyed tables are reconstructed as Lua arrays (1-based indexes).
        // This preserves array semantics across this serialization boundary :^)
        if (!NumericEntries.empty() && ObjectOut.empty() && !HasNonStringOrNumericKey) {
            TDetachedLuaValue::Array ArrayOut(MaxNumericIndex);
            for (const auto& [Index, Value] : NumericEntries) {
                if (Index > 0 && Index <= ArrayOut.size()) {
                    ArrayOut[Index - 1] = Value;
                }
            }
            return { { std::move(ArrayOut) } };
        }
        return { { std::move(ObjectOut) } };
    }
    default:
        throw std::runtime_error("unsupported Lua type for cross-thread snapshot");
    }
}
void TLuaResult::MarkAsReady() {
    mReady = true;
    mReadyCondition.notify_all();
}

void TLuaResult::WaitUntilReady() {
    std::unique_lock Lock(mMutex);
    while (!mReady)
        mReadyCondition.wait_for(Lock, std::chrono::milliseconds(50),
            [this] {
                return mReady;
            });
}

TEST_CASE("TLuaInStateResult MarkReadyError(string) marks ready and wakes waiters") {
    TLuaVoidResult result("state_local");
    std::atomic<bool> waiterDone { false };

    auto waiter = std::thread([&] {
        result.WaitUntilReady();
        waiterDone.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK_FALSE(waiterDone.load(std::memory_order_acquire));

    result.MarkReadyError(std::string("boom"));
    waiter.join();

    CHECK(result.IsReady());
    const auto snapshot = result.GetSnapshot();
    CHECK(snapshot.Ready);
    CHECK(snapshot.Error);
    CHECK(snapshot.ErrorMessage == "boom");
    CHECK(snapshot.StateId == "state_local");
}

TEST_CASE("TLuaInStateResult MarkReadySuccess clears error state") {
    TLuaVoidResult result("state_local_success");
    result.MarkReadySuccess();

    CHECK(result.IsReady());
    CHECK_FALSE(result.IsError());
    const auto snapshot = result.GetSnapshot();
    CHECK(snapshot.Ready);
    CHECK_FALSE(snapshot.Error);
    CHECK(snapshot.ErrorMessage.empty());
}

TEST_CASE("TLuaResult MarkReadyError(string) marks ready and wakes waiters") {
    TLuaResult result("state_a", "fn_a");
    std::atomic<bool> waiterDone { false };

    auto waiter = std::thread([&] {
        result.WaitUntilReady();
        waiterDone.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK_FALSE(waiterDone.load(std::memory_order_acquire));

    result.MarkReadyError(std::string("boom"));
    waiter.join();

    CHECK(result.IsReady());
    const auto snapshot = result.GetDetachedSnapshot();
    CHECK(snapshot.Ready);
    CHECK(snapshot.Error);
    CHECK(snapshot.ErrorMessage == "boom");
    CHECK(snapshot.StateId == "state_a");
    CHECK(snapshot.Function == "fn_a");
}

TEST_CASE("TLuaResult detached snapshot freezes nested string-keyed tables") {
    sol::state lua;
    TLuaResult result("state_table", "fn_table");
    lua.open_libraries(sol::lib::base);

    auto outer = lua.create_table();
    auto inner = lua.create_table();
    inner["k"] = std::string("v");

    outer["flag"] = true;
    outer["msg"] = std::string("hello");
    outer["inner"] = inner;
    outer[1] = std::string("ignored_numeric_key");

    result.MarkReadySuccess(sol::make_object(lua.lua_state(), outer));
    const auto detached = result.GetDetachedSnapshot();

    CHECK(detached.Ready);
    CHECK_FALSE(detached.Error);
    const auto* object = std::get_if<TDetachedLuaValue::Object>(&detached.Result.V);
    REQUIRE(object != nullptr);

    CHECK(object->contains("flag"));
    CHECK(object->contains("msg"));
    CHECK(object->contains("inner"));
    CHECK_FALSE(object->contains("1"));

    const auto* flag = std::get_if<bool>(&object->at("flag")->V);
    REQUIRE(flag != nullptr);
    CHECK(*flag);

    const auto* msg = std::get_if<std::string>(&object->at("msg")->V);
    REQUIRE(msg != nullptr);
    CHECK(*msg == "hello");

    const auto* innerObj = std::get_if<TDetachedLuaValue::Object>(&object->at("inner")->V);
    REQUIRE(innerObj != nullptr);
    REQUIRE(innerObj->contains("k"));
    const auto* innerValue = std::get_if<std::string>(&innerObj->at("k")->V);
    REQUIRE(innerValue != nullptr);
    CHECK(*innerValue == "v");
}

TEST_CASE("TLuaResult detached snapshot preserves numeric array tables") {
    sol::state lua;
    TLuaResult result("state_array", "fn_array");
    lua.open_libraries(sol::lib::base);

    auto arr = lua.create_table();
    arr[1] = std::string("a");
    arr[2] = 42;
    arr[4] = true; // keep sparse indexes

    result.MarkReadySuccess(sol::make_object(lua.lua_state(), arr));
    const auto detached = result.GetDetachedSnapshot();

    CHECK(detached.Ready);
    CHECK_FALSE(detached.Error);

    const auto* array = std::get_if<TDetachedLuaValue::Array>(&detached.Result.V);
    REQUIRE(array != nullptr);
    REQUIRE(array->size() == 4);

    const auto* v1 = std::get_if<std::string>(&(*array)[0].V);
    REQUIRE(v1 != nullptr);
    CHECK(*v1 == "a");

    const auto* v2 = std::get_if<int>(&(*array)[1].V);
    REQUIRE(v2 != nullptr);
    CHECK(*v2 == 42);

    CHECK(std::holds_alternative<std::monostate>((*array)[2].V));

    const auto* v4 = std::get_if<bool>(&(*array)[3].V);
    REQUIRE(v4 != nullptr);
    CHECK(*v4);
}

TEST_CASE("TLuaResult MarkReadySuccess throws on unsupported Lua function value") {
    sol::state lua;
    TLuaResult result("state_fn", "fn_fn");
    lua.open_libraries(sol::lib::base);
    lua["f"] = [] { return 1; };
    const sol::table globals = lua.globals();
    const sol::protected_function fn = globals.get<sol::protected_function>("f");
    const sol::object fnObj = sol::make_object(lua.lua_state(), fn);

    CHECK_THROWS_AS(result.MarkReadySuccess(fnObj), std::runtime_error);
    CHECK_FALSE(result.IsReady());
}

TEST_CASE("TLuaResult MarkReadySuccessNoResult stores monostate") {
    TLuaResult result("state_empty", "fn_empty");
    result.MarkReadySuccessNoResult();

    CHECK(result.IsReady());
    const auto snapshot = result.GetDetachedSnapshot();
    CHECK_FALSE(snapshot.Error);
    CHECK(std::holds_alternative<std::monostate>(snapshot.Result.V));
}
