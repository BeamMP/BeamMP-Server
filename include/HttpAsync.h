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

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <sol/sol.hpp>

// Forward declaration for WebSocket client to keep header lean
namespace httplib {
    namespace ws {
        class WebSocketClient;
    }
}

namespace HttpAsync {

    struct HttpResult {
        enum class Type { COMPLETE, PROGRESS } type;
        
        uint64_t requestId;
        
        // Progress data
        long long current = 0;
        long long total = 0;

        // Response data
        int status = 0;
        std::string body;
        std::map<std::string, std::vector<std::string>> headers; 
    };

    class AsyncHttpProxy : public std::enable_shared_from_this<AsyncHttpProxy> {
    public:
        AsyncHttpProxy(std::string baseUrl, sol::table defaultHeaders);
        ~AsyncHttpProxy() = default;

        // Configuration
        void SetConnectTimeout(int seconds);
        void SetReadTimeout(int seconds);
        void VerifySSL(bool verify);
        void SetDefaultHeaders(sol::table headers);

        // HTTP Methods
        sol::table Get(std::string endpoint, sol::object headers, sol::function cb, sol::object prog);
        sol::table Post(std::string endpoint, sol::object data, sol::object headers, sol::function cb);
        sol::table Put(std::string endpoint, sol::object data, sol::object headers, sol::function cb);
        sol::table Patch(std::string endpoint, sol::object data, sol::object headers, sol::function cb);
        sol::table Delete(std::string endpoint, sol::object headers, sol::function cb);
        sol::table Head(std::string endpoint, sol::object headers, sol::function cb);
        
        // File Operations
        sol::table Download(std::string endpoint, std::string savePath, sol::function cb, sol::object prog);
        sol::table PostFile(std::string endpoint, std::string fieldName, std::string filePath, sol::object headers, sol::function cb);

    private:
        std::map<std::string, std::string> PrepareHeaders(sol::object overrides);
        void PreparePayload(sol::object data, sol::object overrides, std::string& outBody, std::map<std::string, std::string>& outHeaders);

        std::string mBaseUrl;
        std::map<std::string, std::string> mDefaultHeaders;
        int mConnectTimeoutSeconds = 5;
        int mReadTimeoutSeconds = 30;
        bool mVerifySSL = true;
    };

    enum class WSEventType { OPEN, MESSAGE, CLOSE, ERROR_EVENT };

    struct WSEvent {
        WSEventType type;
        std::string payload;
        int closeCode;
    };

    class AsyncWebSocket : public std::enable_shared_from_this<AsyncWebSocket> {
    public:
        static sol::object Create(sol::this_state s, std::string url, sol::object headers);

        AsyncWebSocket(std::string url, sol::table headers, lua_State* state);
        ~AsyncWebSocket();

        void Connect();
        void Send(const std::string& data);
        void Close();
        void VerifySSL(bool verify);

        // Lua Callback Registration
        void OnOpen(sol::object cb);
        void OnMessage(sol::object cb);
        void OnClose(sol::object cb);
        void OnError(sol::object cb);

        void ProcessEvents();
        void Abandon();
        
        [[nodiscard]] lua_State* GetLuaState() const { return L; }

    private:
        void PushEvent(WSEvent ev);

        std::string mUrl;
        lua_State* L;
        std::map<std::string, std::string> mHeaders;
        bool mVerifySSL = true;

        std::thread mThread;
        std::atomic<bool> mIsRunning{false};
        std::atomic<bool> mAbandoned{false};
        
        // Internal httplib pointer and sync
        httplib::ws::WebSocketClient* mClient = nullptr;
        std::mutex mClientMutex;

        // Event Queue
        std::queue<WSEvent> mEvents;
        std::mutex mMutex;

        // Lua Registry References
        int mOnOpenRef = LUA_REFNIL;
        int mOnMessageRef = LUA_REFNIL;
        int mOnCloseRef = LUA_REFNIL;
        int mOnErrorRef = LUA_REFNIL;
    };

    // Module Lifecycle
    void Init();
    void Shutdown();
    void Update(sol::state_view& lua);
    void RegisterBindings(sol::state_view& lua);
    void CleanupState(lua_State* L);

} // namespace HttpAsync