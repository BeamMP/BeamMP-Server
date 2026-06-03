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

#pragma once

#include "Common.h"
#include <condition_variable>
#include <string>
#include <memory>
#include <sol/sol.hpp>

using TLuaStateId = std::string;

struct TDetachedLuaValue {
    using Ptr = std::shared_ptr<TDetachedLuaValue>;
    using Array = std::vector<TDetachedLuaValue>;
    // This weird Ptr indirection is needed because some implementations of libstc++,
    // like the GCC 11 one shipped with ubuntu 22.04, need to know the size of the second
    // member of the pairs that make up the elements of such a map. It's fine in vector.
    using Object = std::unordered_map<std::string, TDetachedLuaValue::Ptr>;
    std::variant<std::monostate, bool, double, int, std::string, Array, Object> V;
};
std::ostream& operator<<(std::ostream& os, const TDetachedLuaValue& value);

/// Result for operations that stay within one Lua state (e.g. script loads).
/// This type intentionally does not store any Lua references or serialized Lua values.
struct TLuaVoidResult {
    struct Snapshot {
        bool Error;
        bool Ready;
        std::string ErrorMessage;
        TLuaStateId StateId;
    };

    explicit TLuaVoidResult(TLuaStateId StateId)
        : mStateId(std::move(StateId)) {}

    /// Marks this result as success & sets it as ready, waking all waiting threads.
    void MarkReadySuccess();
    /// Marks this result as erroneous & sets it as ready, waking all waiting threads.
    void MarkReadyError(sol::protected_function_result Res);
    /// Marks this result as erroneous & sets it as ready, waking all waiting threads.
    void MarkReadyError(std::string Res);
    /// Wait (suspend) until this result is ready.
    void WaitUntilReady();
    bool IsReady() const;
    bool IsError() const;
    Snapshot GetSnapshot() const;

private:
    mutable std::mutex mMutex {};
    bool mReady { false };
    bool mError { false };
    std::string mErrorMessage;
    TLuaStateId mStateId;
    std::condition_variable mReadyCondition {};

    void SetErrorMessageFromResult(sol::protected_function_result& Res) {
        if (Res.valid()) {
            beammp_lua_errorf("Error was not an error");
        }
        if (Res.get_type() == sol::type::string) {
            mErrorMessage = Res.get<sol::error>().what();
        } else {
            mErrorMessage = "(unknown error; error object is not inspectable)";
        }
    }

    void MarkAsReady();
};

struct TLuaResult {
    struct DetachedSnapshot {
        bool Error;
        bool Ready;
        std::string ErrorMessage;
        /// Serialized Lua result. Has no reference to the Lua state, and is safe to use
        /// from any thread that acquires it.
        TDetachedLuaValue Result;
        TLuaStateId StateId;
        std::string Function;
    };


    TLuaResult(TLuaStateId StateId, std::string FunctionName) : mStateId(StateId), mFunction(std::move(FunctionName)) {}

    /// Marks this result as success & sets it as ready, waking all threads waiting on it.
    void MarkReadySuccess(sol::object Res);
    /// Marks this result as successful and ready without serializing a Lua value.
    void MarkReadySuccessNoResult();
    /// Marks this result as erroneous & sets it as ready, waking all threads waiting on it.
    void MarkReadyError(sol::protected_function_result Res);
    /// Marks this result as erroneous & sets it as ready, waking all threads waiting on it.
    void MarkReadyError(std::string Res);
    /// Wait (suspend) until this result is ready. Use `GetDetachedSnapshot` to get the result.
    void WaitUntilReady();
    bool IsReady() const;
    bool IsError() const;
    TLuaStateId OwnerState() const;
    void SetOwnerState(TLuaStateId StateId);

    /// Returns a snapshot with, if not an error, a serialized version of the result
    /// object. This never stores or returns raw Lua references and is safe to use
    /// across threads.
    DetachedSnapshot GetDetachedSnapshot() const;

private:
    mutable std::mutex mMutex {};
    bool mReady { false };
    bool mError { false };
    std::string mErrorMessage;
    TDetachedLuaValue mDetachedResult;
    TLuaStateId mStateId;
    std::string mFunction;
    std::condition_variable mReadyCondition {};

    TDetachedLuaValue Freeze(const sol::object& o, int depth = 0);

    void SetErrorMessageFromResult(sol::protected_function_result& Res) {
        if (Res.valid()) {
            beammp_lua_errorf("Error was not an error");
        }
        if (Res.get_type() == sol::type::string) {
            mErrorMessage = Res.get<sol::error>().what();
        } else {
            mErrorMessage = "(unknown error; error object is not inspectable)";
        }
    }

    void MarkAsReady();
};
