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
#include <cstdint>
#include <sol/sol.hpp>

namespace HttpAsync {

    struct HttpResult {
        enum class Type { COMPLETE, PROGRESS } type;
        
        uint64_t requestId;
        
        int status;
        std::string body;
        std::map<std::string, std::vector<std::string>> headers; 
        
        long long current;
        long long total;
    };

    class AsyncHttpProxy {
    public:
        AsyncHttpProxy(std::string baseUrl, sol::table defaultHeaders);
        ~AsyncHttpProxy() = default;

        void SetConnectTimeout(int seconds);
        void SetReadTimeout(int seconds);
        void VerifySSL(bool verify) { mVerifySSL = verify; }
        void SetDefaultHeaders(sol::table headers);

        sol::table Get(std::string endpoint, sol::object headers, sol::function cb, sol::object prog);
        sol::table Post(std::string endpoint, sol::object data, sol::object headers, sol::function cb);
        sol::table Put(std::string endpoint, sol::object data, sol::object headers, sol::function cb);
        sol::table Patch(std::string endpoint, sol::object data, sol::object headers, sol::function cb);
        sol::table Delete(std::string endpoint, sol::object headers, sol::function cb);
        sol::table Head(std::string endpoint, sol::object headers, sol::function cb);
        
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

    void Init();
    void Shutdown();
    void Update(sol::state_view& lua);
    void RegisterBindings(sol::state_view& lua);
    void CleanupState(lua_State* L);

} // namespace HttpAsync