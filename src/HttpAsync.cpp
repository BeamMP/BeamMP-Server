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

#include "HttpAsync.h"
#include "httplib.h"
#include "Common.h"
#include "LuaAPI.h"
#include <thread>
#include <fstream>
#include <chrono>
#include <atomic>
#include <deque>
#include <memory>
#include <algorithm>
#include <map>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

namespace HttpAsync {

struct PendingRequest {
    lua_State* L;
    int callbackRef = LUA_REFNIL;
    int progressRef = LUA_REFNIL;
    std::atomic<bool> abandoned{false};
};

// --- Centralized Global Context ---
static struct GlobalContext {
    std::deque<HttpResult> results;
    std::mutex resultsMutex;
    
    std::atomic<bool> shuttingDown{false};
    std::unique_ptr<httplib::ThreadPool> threadPool;
    std::atomic<uint64_t> nextRequestId{1};
    
    std::map<uint64_t, std::shared_ptr<PendingRequest>> pendingRequests;
    std::map<lua_State*, int> stateRequestCount;
    std::mutex limitMutex;
    
    std::vector<std::weak_ptr<AsyncWebSocket>> webSockets;
    std::mutex wsMutex;
    std::map<lua_State*, int> stateWsCount;
    
    int actualPoolSize = 16;
    int maxRequestsPerPlugin = 8;
    int maxWsPerPlugin = 4;
    int maxWsGlobal = 32;
    int currentWsGlobal = 0;
} ctx;

static const char* DEFAULT_USER_AGENT = "BeamMP-Server/1.0";

// --- Utilities ---

static void ToLowerInPlace(std::string& s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }
}

static std::string ToLower(std::string s) {
    ToLowerInPlace(s);
    return s;
}

static void UnrefCallback(lua_State* L, int& ref) {
    if (ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        ref = LUA_REFNIL;
    }
}

static void ReleasePendingRequest(const std::shared_ptr<PendingRequest>& info) {
    if (!info) return;
    UnrefCallback(info->L, info->callbackRef);
    UnrefCallback(info->L, info->progressRef);
}

static int MakeRef(sol::object obj) {
    if (!obj.is<sol::function>()) return LUA_REFNIL;
    lua_State* L = obj.lua_state();
    obj.push();
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

template<typename... Args>
static void InvokeLuaCallback(lua_State* L, int ref, const char* errorContext, Args&&... args) {
    if (ref == LUA_REFNIL) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    sol::protected_function cb = sol::stack::pop<sol::protected_function>(L);
    if (cb.valid()) {
        auto r = cb(std::forward<Args>(args)...);
        if (!r.valid()) beammp_lua_errorf("%s: %s", errorContext, sol::error(r).what());
    }
}

static void PushResult(HttpResult res) {
    if (ctx.shuttingDown.load()) return;
    std::lock_guard<std::mutex> lock(ctx.resultsMutex);
    ctx.results.push_back(std::move(res));
}

static void ExtractHeaders(const httplib::Headers& source, std::map<std::string, std::vector<std::string>>& dest) {
    for (const auto& [k, v] : source) dest[k].push_back(v);
}

static bool ParseUrl(const std::string& url, std::string& base, std::string& path) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return false;

    auto pos = url.find("://");
    if (pos == std::string::npos) return false;
    
    auto pathPos = url.find_first_of("/?#", pos + 3);
    if (pathPos == std::string::npos) {
        base = url;
        path = "/";
    } else {
        base = url.substr(0, pathPos);
        path = url.substr(pathPos);
    }
    
    if (base.length() <= pos + 3) return false;

    return true;
}

static bool IsValidWsUrl(const std::string& url) {
    return url.rfind("ws://", 0) == 0 || url.rfind("wss://", 0) == 0;
}

// --- HTTP Implementation ---

static sol::table CreateHandle(lua_State* L, std::shared_ptr<PendingRequest> info) {
    sol::state_view lua(L);
    sol::table handle = lua.create_table();
    
    if (info) {
        handle["Cancel"] = [info]() {
            info->abandoned.store(true);
            ReleasePendingRequest(info);
        };
        handle["IsActive"] = [info]() { 
            return !info->abandoned.load(); 
        };
        handle["OnProgress"] = [info](sol::object func) {
            if (func.is<sol::function>()) {
                UnrefCallback(info->L, info->progressRef);
                info->progressRef = MakeRef(func);
            }
        };
    } else {
        handle["Error"] = "Rate limited or Shutdown";
    }
    return handle;
}

static bool SetupClient(const std::string& url, int connectTimeout, int readTimeout, bool verifySSL, std::unique_ptr<httplib::Client>& outClient, std::string& outPath) {
    std::string base;
    if (!ParseUrl(url, base, outPath)) return false;

    outClient = std::make_unique<httplib::Client>(base);
    outClient->set_connection_timeout(connectTimeout, 0);
    outClient->set_read_timeout(readTimeout, 0);
    outClient->set_write_timeout(readTimeout, 0);
    outClient->set_follow_location(true);
    outClient->enable_server_certificate_verification(verifySSL);
    return true;
}

static std::shared_ptr<PendingRequest> EnqueueTask(lua_State* L, int cbRef, int progRef, std::function<void(uint64_t, std::shared_ptr<PendingRequest>)> task) {
    if (ctx.shuttingDown.load() || !ctx.threadPool) {
        UnrefCallback(L, cbRef);
        UnrefCallback(L, progRef);
        return nullptr;
    }
    
    {
        std::lock_guard<std::mutex> lock(ctx.limitMutex);
        if (ctx.stateRequestCount[L] >= ctx.maxRequestsPerPlugin) {
            beammp_lua_warnf("Plugin reached HTTP request limit ({}). Request rejected.", ctx.maxRequestsPerPlugin);
            UnrefCallback(L, cbRef);
            UnrefCallback(L, progRef);
            return nullptr;
        }
        ctx.stateRequestCount[L]++;
    }

    uint64_t reqId = ctx.nextRequestId++;
    auto info = std::make_shared<PendingRequest>();
    info->L = L;
    info->callbackRef = cbRef;
    info->progressRef = progRef;

    {
        std::lock_guard<std::mutex> lock(ctx.resultsMutex);
        ctx.pendingRequests[reqId] = info;
    }

    ctx.threadPool->enqueue([reqId, info, task = std::move(task)]() {
        task(reqId, info);
    });

    return info;
}

static sol::table Dispatch(std::string method, std::string url, std::map<std::string, std::string> headers,
                           std::string body, int connectTimeout, int readTimeout, bool verifySSL, lua_State* L, int cbRef, int progRef) {
    
    auto info = EnqueueTask(L, cbRef, progRef,[=, b = std::move(body), hMap = std::move(headers)]
                                               (uint64_t reqId, std::shared_ptr<PendingRequest> pReq) {
        std::string path;
        std::unique_ptr<httplib::Client> cli;
        
        if (!SetupClient(url, connectTimeout, readTimeout, verifySSL, cli, path)) {
            HttpResult res{HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "Invalid URL", {}};
            PushResult(std::move(res)); 
            return;
        }

        httplib::Headers h;
        bool hasUA = false;
        std::string cType = "application/json";
        
        for (const auto&[key, val] : hMap) {
            std::string kLower = ToLower(key);
            if (kLower == "user-agent") hasUA = true;
            if (kLower == "content-type") {
                cType = val;
                if (method == "POST" || method == "PUT" || method == "PATCH") continue; 
            }
            h.emplace(key, val);
        }
        if (!hasUA) h.emplace("User-Agent", DEFAULT_USER_AGENT);

        auto lastProg = std::chrono::steady_clock::now();
        auto prog_func = [&](uint64_t len, uint64_t total) {
            if (ctx.shuttingDown.load() || pReq->abandoned.load()) return false; 
            
            if (pReq->progressRef != LUA_REFNIL) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProg).count() > 100 || len == total) {
                    HttpResult res{HttpResult::Type::PROGRESS, reqId, static_cast<long long>(len), static_cast<long long>(total), 0, "", {}};
                    PushResult(std::move(res)); 
                    lastProg = now;
                }
            }
            return true;
        };

        httplib::Result response;
        if      (method == "POST")   response = cli->Post(path.c_str(), h, b, cType.c_str(), prog_func);
        else if (method == "PUT")    response = cli->Put(path.c_str(), h, b, cType.c_str(), prog_func);
        else if (method == "PATCH")  response = cli->Patch(path.c_str(), h, b, cType.c_str(), prog_func);
        else if (method == "DELETE") response = cli->Delete(path.c_str(), h, prog_func);
        else if (method == "HEAD")   response = cli->Head(path.c_str(), h);
        else                         response = cli->Get(path.c_str(), h, prog_func);

        if (pReq->abandoned.load()) return;

        HttpResult res{HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "", {}};
        if (response) {
            res.status = response->status;
            res.body = std::move(response->body);
            ExtractHeaders(response->headers, res.headers);
        } else {
            res.body = "Network Error: " + httplib::to_string(response.error());
        }
        PushResult(std::move(res));
    });

    return CreateHandle(L, info);
}

// --- AsyncHttpProxy ---

AsyncHttpProxy::AsyncHttpProxy(std::string baseUrl, sol::table defaultHeaders) : mBaseUrl(std::move(baseUrl)) {
    SetDefaultHeaders(defaultHeaders);
}

void AsyncHttpProxy::SetConnectTimeout(int seconds) { mConnectTimeoutSeconds = seconds; }
void AsyncHttpProxy::SetReadTimeout(int seconds) { mReadTimeoutSeconds = seconds; }
void AsyncHttpProxy::VerifySSL(bool verify) { mVerifySSL = verify; }

void AsyncHttpProxy::SetDefaultHeaders(sol::table headers) {
    mDefaultHeaders.clear();
    if (headers != sol::lua_nil && headers.valid()) {
        for (auto const& pair : headers) {
            if (pair.first.is<std::string>() && pair.second.is<std::string>())
                mDefaultHeaders[pair.first.as<std::string>()] = pair.second.as<std::string>();
        }
    }
}

std::map<std::string, std::string> AsyncHttpProxy::PrepareHeaders(sol::object overrides) {
    auto finalHeaders = mDefaultHeaders;
    if (overrides.is<sol::table>()) {
        for (auto const& pair : overrides.as<sol::table>()) {
            if (pair.first.is<std::string>() && pair.second.is<std::string>())
                finalHeaders[pair.first.as<std::string>()] = pair.second.as<std::string>();
        }
    }
    return finalHeaders;
}

void AsyncHttpProxy::PreparePayload(sol::object data, sol::object overrides, std::string& outBody, std::map<std::string, std::string>& outHeaders) {
    outHeaders = PrepareHeaders(overrides);
    
    bool hasCT = false;
    for (const auto& [k, v] : outHeaders) {
        if (ToLower(k) == "content-type") hasCT = true;
    }

    if (data.is<sol::table>()) {
        outBody = LuaAPI::MP::JsonEncode(data.as<sol::table>());
        if (!hasCT) outHeaders["Content-Type"] = "application/json";
    } else {
        outBody = data.is<std::string>() ? data.as<std::string>() : "";
    }
}

sol::table AsyncHttpProxy::Get(std::string ep, sol::object h, sol::function cb, sol::object prog) {
    return Dispatch("GET", mBaseUrl + ep, PrepareHeaders(h), "", mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cb.lua_state(), MakeRef(cb), MakeRef(prog));
}

sol::table AsyncHttpProxy::Post(std::string ep, sol::object data, sol::object h, sol::function cb) {
    std::string body; std::map<std::string, std::string> headers;
    PreparePayload(data, h, body, headers);
    return Dispatch("POST", mBaseUrl + ep, headers, std::move(body), mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cb.lua_state(), MakeRef(cb), LUA_REFNIL);
}

sol::table AsyncHttpProxy::Put(std::string ep, sol::object data, sol::object h, sol::function cb) {
    std::string body; std::map<std::string, std::string> headers;
    PreparePayload(data, h, body, headers);
    return Dispatch("PUT", mBaseUrl + ep, headers, std::move(body), mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cb.lua_state(), MakeRef(cb), LUA_REFNIL);
}

sol::table AsyncHttpProxy::Patch(std::string ep, sol::object data, sol::object h, sol::function cb) {
    std::string body; std::map<std::string, std::string> headers;
    PreparePayload(data, h, body, headers);
    return Dispatch("PATCH", mBaseUrl + ep, headers, std::move(body), mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cb.lua_state(), MakeRef(cb), LUA_REFNIL);
}

sol::table AsyncHttpProxy::Delete(std::string ep, sol::object h, sol::function cb) {
    return Dispatch("DELETE", mBaseUrl + ep, PrepareHeaders(h), "", mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cb.lua_state(), MakeRef(cb), LUA_REFNIL);
}

sol::table AsyncHttpProxy::Head(std::string ep, sol::object h, sol::function cb) {
    return Dispatch("HEAD", mBaseUrl + ep, PrepareHeaders(h), "", mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cb.lua_state(), MakeRef(cb), LUA_REFNIL);
}

sol::table AsyncHttpProxy::PostFile(std::string ep, std::string fieldName, std::string filePath, sol::object headers, sol::function cb) {
    auto hMap = PrepareHeaders(headers);
    auto info = EnqueueTask(cb.lua_state(), MakeRef(cb), LUA_REFNIL, 
    [this, ep, fieldName, filePath, hMap = std::move(hMap)](uint64_t reqId, std::shared_ptr<PendingRequest> pReq) mutable {
        std::string path;
        std::unique_ptr<httplib::Client> cli;
        
        if (!SetupClient(mBaseUrl + ep, mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cli, path)) {
            PushResult({HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "Invalid URL", {}}); 
            return;
        }
        
        if (!fs::exists(filePath)) {
            PushResult({HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "File not found", {}}); 
            return;
        }

        auto file_stream = std::make_shared<std::ifstream>(filePath, std::ios::binary);
        if (!file_stream || !file_stream->is_open()) {
            PushResult({HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "Could not open file", {}}); 
            return;
        }

        httplib::UploadFormDataItems regular_items; 
        httplib::FormDataProviderItems provider_items = {
            {
                fieldName,
                [file_stream, pReq](size_t offset, httplib::DataSink &sink) {
                    if (pReq->abandoned.load()) return false;

                    if (static_cast<size_t>(file_stream->tellg()) != offset) {
                        file_stream->clear(); 
                        file_stream->seekg(static_cast<std::streamoff>(offset), std::ios::beg);
                    }
                    
                    char buffer[8192];
                    file_stream->read(buffer, sizeof(buffer));
                    std::streamsize read_bytes = file_stream->gcount();
                    if (read_bytes > 0) sink.write(buffer, static_cast<size_t>(read_bytes));
                    if (file_stream->eof()) sink.done();
                    return true;
                },
                fs::path(filePath).filename().string(),
                "application/octet-stream"
            }
        };

        httplib::Headers finalH;
        bool hasUA = false;
        for (const auto& [key, val] : hMap) {
            std::string kLower = ToLower(key);
            if (kLower == "user-agent") hasUA = true;
            if (kLower == "content-type") continue; 
            finalH.emplace(key, val);
        }
        if (!hasUA) finalH.emplace("User-Agent", DEFAULT_USER_AGENT);
        
        auto response = cli->Post(path.c_str(), finalH, regular_items, provider_items);
        HttpResult res{HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "", {}};
        
        if (response) {
            res.status = response->status;
            res.body = std::move(response->body);
            ExtractHeaders(response->headers, res.headers);
        } else {
            res.body = "Upload Failed: " + httplib::to_string(response.error());
        }
        PushResult(std::move(res));
    });

    return CreateHandle(cb.lua_state(), info);
}

sol::table AsyncHttpProxy::Download(std::string ep, std::string savePath, sol::function cb, sol::object prog) {
    auto hMap = PrepareHeaders(sol::lua_nil);
    auto info = EnqueueTask(cb.lua_state(), MakeRef(cb), MakeRef(prog), 
    [this, ep, savePath, hMap = std::move(hMap)](uint64_t reqId, std::shared_ptr<PendingRequest> pReq) mutable {
        std::string path;
        std::unique_ptr<httplib::Client> cli;
        
        if (!SetupClient(mBaseUrl + ep, mConnectTimeoutSeconds, mReadTimeoutSeconds, mVerifySSL, cli, path)) {
            PushResult({HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "Invalid URL", {}}); 
            return;
        }
        
        std::ofstream ofs(savePath, std::ios::binary);
        if (!ofs) {
            PushResult({HttpResult::Type::COMPLETE, reqId, 0, 0, 0, "Could not open file for writing", {}}); 
            return;
        }
        
        httplib::Headers finalH;
        bool hasUA = false;
        for (const auto&[key, val] : hMap) {
            if (ToLower(key) == "user-agent") hasUA = true;
            finalH.emplace(key, val);
        }
        if (!hasUA) finalH.emplace("User-Agent", DEFAULT_USER_AGENT);

        int status_code = 0; 
        std::map<std::string, std::vector<std::string>> resHeaders;
        auto lastProg = std::chrono::steady_clock::now();
        
        auto res = cli->Get(path.c_str(), finalH, 
            [&](const httplib::Response &r) { 
                status_code = r.status; 
                ExtractHeaders(r.headers, resHeaders);
                return !ctx.shuttingDown.load() && !pReq->abandoned.load(); 
            },
            [&](const char *b, size_t l) { 
                if (ctx.shuttingDown.load() || pReq->abandoned.load()) return false; 
                ofs.write(b, static_cast<std::streamsize>(l)); 
                return true; 
            },
            [&](uint64_t len, uint64_t total) {
                if (pReq->progressRef != LUA_REFNIL && !pReq->abandoned.load()) {
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProg).count() > 100 || len == total) {
                        PushResult({HttpResult::Type::PROGRESS, reqId, static_cast<long long>(len), static_cast<long long>(total), 0, "", {}}); 
                        lastProg = now;
                    }
                }
                return true;
            }
        );
        ofs.close();
        
        HttpResult fres{HttpResult::Type::COMPLETE, reqId, 0, 0, status_code, "", std::move(resHeaders)};
        fres.body = res ? "Success" : "Download Failed: " + httplib::to_string(res.error());
        PushResult(std::move(fres));
    });

    return CreateHandle(cb.lua_state(), info);
}


// --- AsyncWebSocket ---

AsyncWebSocket::AsyncWebSocket(std::string url, sol::table headers, lua_State* state) 
    : mUrl(std::move(url)), L(state) {
    if (headers.valid()) {
        for (auto const& pair : headers) {
            if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
                mHeaders.emplace(pair.first.as<std::string>(), pair.second.as<std::string>());
            }
        }
    }
}

AsyncWebSocket::~AsyncWebSocket() {
    Abandon();
    if (mThread.joinable()) mThread.join();
}

sol::object AsyncWebSocket::Create(sol::this_state s, std::string url, sol::object headers) {
    lua_State* L = s.lua_state();

    {
        std::lock_guard<std::mutex> lock(ctx.limitMutex);
        if (ctx.currentWsGlobal >= ctx.maxWsGlobal || ctx.stateWsCount[L] >= ctx.maxWsPerPlugin) {
            beammp_lua_warnf("WebSocket limit reached (Global: {}/{}, Plugin: {}/{}).", 
                ctx.currentWsGlobal, ctx.maxWsGlobal, ctx.stateWsCount[L], ctx.maxWsPerPlugin);
            return sol::make_object(s, sol::lua_nil);
        }
        ctx.stateWsCount[L]++;
        ctx.currentWsGlobal++;
    }

    if (!IsValidWsUrl(url)) {
        beammp_lua_warnf("Invalid WebSocket URL: {}. Use 'ws://' or 'wss://'.", url);
        std::lock_guard<std::mutex> lock(ctx.limitMutex);
        ctx.stateWsCount[L]--;
        ctx.currentWsGlobal--;
        return sol::make_object(s, sol::lua_nil);
    }

    auto ws = std::make_shared<AsyncWebSocket>(url, headers.is<sol::table>() ? headers.as<sol::table>() : sol::table(), L);
    
    std::lock_guard<std::mutex> lock(ctx.wsMutex);
    ctx.webSockets.push_back(ws);
    return sol::make_object(s, ws);
}

void AsyncWebSocket::VerifySSL(bool verify) { mVerifySSL = verify; }

void AsyncWebSocket::Connect() {
    if (mIsRunning.exchange(true)) return;
    
    mThread = std::thread([this]() {
        httplib::Headers h;
        bool hasUA = false;

        for (const auto& [k, v] : mHeaders) {
            if (ToLower(k) == "user-agent") hasUA = true;
            h.emplace(k, v);
        }
        if (!hasUA) h.emplace("User-Agent", DEFAULT_USER_AGENT);

        httplib::ws::WebSocketClient client(mUrl, h);
        client.enable_server_certificate_verification(mVerifySSL);
        
        {
            std::lock_guard<std::mutex> lock(mClientMutex);
            if (mAbandoned) return;
            mClient = &client;
        }

        if (!client.connect()) {
            PushEvent({WSEventType::ERROR_EVENT, "Failed to connect", 0});
            mIsRunning = false;
            std::lock_guard<std::mutex> lock(mClientMutex);
            mClient = nullptr;
            return;
        }

        PushEvent({WSEventType::OPEN, "", 0});

        std::string msg;
        while (mIsRunning && !mAbandoned) {
            if (client.read(msg) == httplib::ws::ReadResult::Fail) break;
            PushEvent({WSEventType::MESSAGE, msg, 0});
            msg.clear();
        }

        PushEvent({WSEventType::CLOSE, "Connection closed", 1000});
        mIsRunning = false;
        
        std::lock_guard<std::mutex> lock(mClientMutex);
        mClient = nullptr;
    });
}

void AsyncWebSocket::Send(const std::string& data) {
    std::lock_guard<std::mutex> lock(mClientMutex);
    if (mClient && mClient->is_open()) mClient->send(data);
}

void AsyncWebSocket::Close() {
    mIsRunning = false;
    std::lock_guard<std::mutex> lock(mClientMutex);
    if (mClient && mClient->is_open()) mClient->close();
}

void AsyncWebSocket::PushEvent(WSEvent ev) {
    if (mAbandoned) return;
    std::lock_guard<std::mutex> lock(mMutex);
    mEvents.push(std::move(ev));
}

void AsyncWebSocket::OnOpen(sol::object cb)    { UnrefCallback(L, mOnOpenRef);    mOnOpenRef = MakeRef(cb); }
void AsyncWebSocket::OnMessage(sol::object cb) { UnrefCallback(L, mOnMessageRef); mOnMessageRef = MakeRef(cb); }
void AsyncWebSocket::OnClose(sol::object cb)   { UnrefCallback(L, mOnCloseRef);   mOnCloseRef = MakeRef(cb); }
void AsyncWebSocket::OnError(sol::object cb)   { UnrefCallback(L, mOnErrorRef);   mOnErrorRef = MakeRef(cb); }

void AsyncWebSocket::ProcessEvents() {
    if (mAbandoned) return;

    std::queue<WSEvent> events;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        std::swap(events, mEvents);
    }

    while (!events.empty() && !mAbandoned) {
        auto ev = events.front();
        events.pop();

        try {
            switch (ev.type) {
                case WSEventType::OPEN:
                    InvokeLuaCallback(L, mOnOpenRef, "WS OnOpen Error");
                    break;
                case WSEventType::MESSAGE:
                    InvokeLuaCallback(L, mOnMessageRef, "WS OnMessage Error", ev.payload);
                    break;
                case WSEventType::CLOSE:
                    InvokeLuaCallback(L, mOnCloseRef, "WS OnClose Error", ev.closeCode, ev.payload);
                    break;
                case WSEventType::ERROR_EVENT:
                    InvokeLuaCallback(L, mOnErrorRef, "WS OnError Error", ev.payload);
                    break;
                default:
                    break; 
            }
        } catch (const std::exception& e) {
            beammp_lua_errorf("WebSocket Exception: {}", e.what());
        }
    }
}

void AsyncWebSocket::Abandon() {
    if (mAbandoned.exchange(true)) return;
    
    {
        std::lock_guard<std::mutex> lock(ctx.limitMutex);
        ctx.stateWsCount[L]--;
        if (ctx.stateWsCount[L] <= 0) ctx.stateWsCount.erase(L);
        ctx.currentWsGlobal--;
    }

    Close();
    
    UnrefCallback(L, mOnOpenRef);
    UnrefCallback(L, mOnMessageRef);
    UnrefCallback(L, mOnCloseRef);
    UnrefCallback(L, mOnErrorRef);
}

// --- Lifecycle & Lua Bindings ---

void RegisterBindings(sol::state_view& lua) {
    lua.new_usertype<AsyncHttpProxy>("AsyncHttp", sol::no_constructor,
        "SetConnectTimeout", &AsyncHttpProxy::SetConnectTimeout,
        "SetReadTimeout", &AsyncHttpProxy::SetReadTimeout,
        "VerifySSL", &AsyncHttpProxy::VerifySSL,
        "SetDefaultHeaders", &AsyncHttpProxy::SetDefaultHeaders,
        "Get", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object h, sol::function cb) { return self.Get(ep, h, cb, sol::nil); }, &AsyncHttpProxy::Get),
        "Post", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object d, sol::function cb) { return self.Post(ep, d, sol::nil, cb); }, &AsyncHttpProxy::Post),
        "PostFile", sol::overload([](AsyncHttpProxy& self, std::string ep, std::string fn, std::string fp, sol::function cb) { return self.PostFile(ep, fn, fp, sol::nil, cb); }, &AsyncHttpProxy::PostFile),
        "Put", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object d, sol::function cb) { return self.Put(ep, d, sol::nil, cb); }, &AsyncHttpProxy::Put),
        "Patch", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object d, sol::function cb) { return self.Patch(ep, d, sol::nil, cb); }, &AsyncHttpProxy::Patch),
        "Delete", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::function cb) { return self.Delete(ep, sol::nil, cb); }, &AsyncHttpProxy::Delete),
        "Head", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::function cb) { return self.Head(ep, sol::nil, cb); }, &AsyncHttpProxy::Head),
        "Download", sol::overload([](AsyncHttpProxy& self, std::string ep, std::string p, sol::function cb) { return self.Download(ep, p, cb, sol::nil); }, &AsyncHttpProxy::Download)
    );

    lua["AsyncHttp"]["new"] = sol::overload([](std::string url) { return std::make_shared<AsyncHttpProxy>(url, sol::table(sol::lua_nil)); },[](std::string url, sol::table headers) { return std::make_shared<AsyncHttpProxy>(url, headers); }
    );

    lua.new_usertype<AsyncWebSocket>("AsyncWebSocket", sol::no_constructor,
        "Connect", &AsyncWebSocket::Connect,
        "Send", &AsyncWebSocket::Send,
        "Close", &AsyncWebSocket::Close,
        "VerifySSL", &AsyncWebSocket::VerifySSL,
        "OnOpen", &AsyncWebSocket::OnOpen,
        "OnMessage", &AsyncWebSocket::OnMessage,
        "OnClose", &AsyncWebSocket::OnClose,
        "OnError", &AsyncWebSocket::OnError
    );

    lua["AsyncWebSocket"]["new"] = &AsyncWebSocket::Create;
}

void Update(sol::state_view& lua) {
    lua_State* L = lua.lua_state();
    std::deque<HttpResult> toProcess;
    
    // 1. Gather relevant results safely
    {
        std::lock_guard<std::mutex> lock(ctx.resultsMutex);
        auto it = ctx.results.begin();
        while (it != ctx.results.end()) {
            auto reqIt = ctx.pendingRequests.find(it->requestId);
            if (reqIt == ctx.pendingRequests.end()) {
                it = ctx.results.erase(it);
            } else if (reqIt->second->L == L) {
                toProcess.push_back(std::move(*it));
                it = ctx.results.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 2. Dispatch Lua callbacks
    for (const auto& res : toProcess) {
        std::shared_ptr<PendingRequest> info;
        {
            std::lock_guard<std::mutex> lock(ctx.resultsMutex);
            auto it = ctx.pendingRequests.find(res.requestId);
            if (it != ctx.pendingRequests.end()) info = it->second;
        }

        if (!info || info->abandoned.load()) {
            if (info) {
                ReleasePendingRequest(info);

                std::lock_guard<std::mutex> lock(ctx.limitMutex);
                ctx.stateRequestCount[L]--;
            }
            std::lock_guard<std::mutex> lock(ctx.resultsMutex);
            ctx.pendingRequests.erase(res.requestId);
            continue; 
        }

        if (res.type == HttpResult::Type::PROGRESS) {
            InvokeLuaCallback(L, info->progressRef, "AsyncHttp Progress Error", res.current, res.total);
        } else {
            if (info->callbackRef != LUA_REFNIL) {
                sol::table luaHeaders = lua.create_table();
                for (auto const& [name, values] : res.headers) {
                    if (values.empty()) continue;
                    std::string key = ToLower(name); 
                    if (values.size() > 1 || key == "set-cookie") luaHeaders[key] = sol::as_table(values);
                    else luaHeaders[key] = values[0];
                }
                InvokeLuaCallback(L, info->callbackRef, "AsyncHttp Callback Error", res.status, res.body, luaHeaders);
            }
            
            ReleasePendingRequest(info);
            
            {
                std::lock_guard<std::mutex> lock(ctx.limitMutex);
                ctx.stateRequestCount[L]--;
            }
            
            std::lock_guard<std::mutex> lock(ctx.resultsMutex);
            ctx.pendingRequests.erase(res.requestId);
        }
    }

    // 3. Update WebSockets
    std::vector<std::shared_ptr<AsyncWebSocket>> websocketsToUpdate;
    {
        std::lock_guard<std::mutex> wsLock(ctx.wsMutex);
        auto wsIt = ctx.webSockets.begin();
        while (wsIt != ctx.webSockets.end()) {
            if (auto ws = wsIt->lock()) {
                if (ws->GetLuaState() == L) websocketsToUpdate.push_back(ws);
                ++wsIt;
            } else {
                wsIt = ctx.webSockets.erase(wsIt);
            }
        }
    }

    for (auto& ws : websocketsToUpdate) {
        ws->ProcessEvents();
    }
}

void CleanupState(lua_State* L) {
    // Purge pending HTTP requests
    {
        std::lock_guard<std::mutex> lock(ctx.resultsMutex);
        for (auto it = ctx.pendingRequests.begin(); it != ctx.pendingRequests.end(); ) {
            if (it->second->L == L) {
                it->second->abandoned.store(true);
                ReleasePendingRequest(it->second);
                it = ctx.pendingRequests.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Purge attached WebSockets
    {
        std::lock_guard<std::mutex> wsLock(ctx.wsMutex);
        auto wsIt = ctx.webSockets.begin();
        while (wsIt != ctx.webSockets.end()) {
            if (auto ws = wsIt->lock()) {
                if (ws->GetLuaState() == L) {
                    ws->Abandon();
                    wsIt = ctx.webSockets.erase(wsIt);
                } else {
                    ++wsIt;
                }
            } else {
                wsIt = ctx.webSockets.erase(wsIt);
            }
        }
    }
}

void Init() { 
    ctx.shuttingDown.store(false); 
    int cores = static_cast<int>(std::thread::hardware_concurrency());
    if (cores <= 0) cores = 4;

    ctx.actualPoolSize = std::clamp(cores * 4, 16, 128);
    ctx.maxRequestsPerPlugin = std::max(ctx.actualPoolSize / 2, 5);
    ctx.threadPool = std::make_unique<httplib::ThreadPool>(ctx.actualPoolSize);

    ctx.maxWsGlobal = std::max(cores * 8, 32);
    ctx.maxWsPerPlugin = std::max(ctx.maxWsGlobal / 4, 4);

    beammp_infof("AsyncHttp initialized. HTTP Pool: {} ({} per plugin). WS Quota: {} ({} per plugin).",
                 ctx.actualPoolSize, ctx.maxRequestsPerPlugin, ctx.maxWsGlobal, ctx.maxWsPerPlugin);
}

void Shutdown() { 
    ctx.shuttingDown.store(true); 
    
    if (ctx.threadPool) {
        ctx.threadPool->shutdown(); 
        ctx.threadPool.reset(); 
    }
    
    {
        std::lock_guard<std::mutex> lock(ctx.resultsMutex);
        ctx.pendingRequests.clear();
        ctx.results.clear();
    }

    {
        std::lock_guard<std::mutex> wsLock(ctx.wsMutex);
        for (auto& weak_ws : ctx.webSockets) {
            if (auto ws = weak_ws.lock()) ws->Abandon();
        }
        ctx.webSockets.clear();
    }
}

} // namespace HttpAsync