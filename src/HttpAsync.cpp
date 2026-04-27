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
#include <regex>
#include <memory>
#include <algorithm>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;

namespace HttpAsync {

struct PendingRequest {
    lua_State* L;
    int callbackRef = LUA_REFNIL;
    int progressRef = LUA_REFNIL;
    std::atomic<bool> abandoned{false};
};

static std::deque<HttpResult> g_Results;
static std::mutex g_Mutex;
static std::atomic<bool> g_ShuttingDown{false};
static std::unique_ptr<httplib::ThreadPool> g_ThreadPool;
static std::atomic<uint64_t> g_NextRequestId{1};
static std::map<uint64_t, std::shared_ptr<PendingRequest>> g_PendingRequests;
static std::map<lua_State*, int> g_StateRequestCount;
static std::mutex g_LimitMutex;

const int MAX_REQUESTS_PER_STATE = 20;
const int THREAD_POOL_SIZE = 8;

static sol::table CreateHandle(lua_State* L, std::shared_ptr<PendingRequest> info) {
    sol::state_view lua(L);
    sol::table handle = lua.create_table();
    if (info) {
        handle["Cancel"] = [info]() { info->abandoned.store(true); };
        handle["IsActive"] = [info]() { return !info->abandoned.load(); };
        
        // This allows the modder to attach a progress listener to the handle
        handle["OnProgress"] = [L, info](sol::object func) {
            if (func.is<sol::function>()) {
                if (info->progressRef != LUA_REFNIL) {
                    luaL_unref(L, LUA_REGISTRYINDEX, info->progressRef);
                }
                func.push();
                info->progressRef = luaL_ref(L, LUA_REGISTRYINDEX);
            }
        };
    } else {
        handle["Error"] = "Rate limited or Shutdown";
    }
    return handle;
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

static void PushResult(HttpResult res) {
    if (g_ShuttingDown.load()) return;
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Results.push_back(std::move(res));
}

static int MakeRef(sol::object obj) {
    if (!obj.is<sol::function>()) return LUA_REFNIL;
    lua_State* L = obj.lua_state();
    obj.push();
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static void ExtractHeaders(const httplib::Headers& source, std::map<std::string, std::vector<std::string>>& dest) {
    for (const auto& [k, v] : source) {
        dest[k].push_back(v);
    }
}

static bool SetupClient(const std::string& url, int connectTimeout, int readTimeout, bool verifySSL, std::unique_ptr<httplib::Client>& outClient, std::string& outPath) {
    static const std::regex url_regex(R"(^(https?://[^/]+)(/.*)?$)", std::regex::extended);
    std::smatch match;
    if (!std::regex_match(url, match, url_regex)) return false;

    outClient = std::make_unique<httplib::Client>(match[1].str());
    outPath = match[2].length() == 0 ? "/" : match[2].str();

    outClient->set_connection_timeout(connectTimeout, 0);
    outClient->set_read_timeout(readTimeout, 0);
    outClient->set_write_timeout(readTimeout, 0);
    outClient->set_follow_location(true);
    outClient->enable_server_certificate_verification(verifySSL);
    return true;
}

static std::shared_ptr<PendingRequest> EnqueueTask(lua_State* L, int cbRef, int progRef, std::function<void(uint64_t, std::shared_ptr<PendingRequest>)> task) {
    if (g_ShuttingDown.load() || !g_ThreadPool) {
        if (cbRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
        if (progRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, progRef);
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(g_LimitMutex);
        if (g_StateRequestCount[L] >= MAX_REQUESTS_PER_STATE) {
            beammp_lua_warnf("Plugin reached HTTP request limit ({}). Request rejected.", MAX_REQUESTS_PER_STATE);
            if (cbRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
            if (progRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, progRef);
            return nullptr;
        }
        g_StateRequestCount[L]++;
    }

    uint64_t reqId = g_NextRequestId++;
    auto info = std::make_shared<PendingRequest>();
    info->L = L;
    info->callbackRef = cbRef;
    info->progressRef = progRef;

    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PendingRequests[reqId] = info;
    }

    g_ThreadPool->enqueue([L, reqId, info, task = std::move(task)]() {
        task(reqId, info);
        
        std::lock_guard<std::mutex> lock(g_LimitMutex);
        g_StateRequestCount[L]--;
    });

    return info;
}

static sol::table Dispatch(std::string method, std::string url, std::map<std::string, std::string> headers,
                        std::string body, int connectTimeout, int readTimeout, bool verifySSL, lua_State* L, int cbRef, int progRef) {
    
    auto info = EnqueueTask(L, cbRef, progRef, [=, b = std::move(body), hMap = std::move(headers)]
                                   (uint64_t reqId, std::shared_ptr<PendingRequest> pReq) {
        std::string path;
        std::unique_ptr<httplib::Client> cli;
        if (!SetupClient(url, connectTimeout, readTimeout, verifySSL, cli, path)) return;

        httplib::Headers h;
        bool hasUA = false;
        std::string cType = "application/json";
        
        for (auto const&[key, val] : hMap) {
            if (ToLower(key) == "user-agent") hasUA = true;
            if (ToLower(key) == "content-type") {
                cType = val;
                if (method == "POST" || method == "PUT" || method == "PATCH") continue; 
            }
            h.emplace(key, val);
        }
        if (!hasUA) h.emplace("User-Agent", "BeamMP-Server/1.0");

        auto lastProg = std::chrono::steady_clock::now();
        auto prog_func = [&](uint64_t len, uint64_t total) {
            if (g_ShuttingDown.load() || pReq->abandoned.load()) return false; 
            if (pReq->progressRef != LUA_REFNIL) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProg).count() > 100 || len == total) {
                    HttpResult res; res.type = HttpResult::Type::PROGRESS; res.requestId = reqId;
                    res.current = static_cast<long long>(len); res.total = static_cast<long long>(total);
                    PushResult(std::move(res)); lastProg = now;
                }
            }
            return true;
        };

        httplib::Result response;
        if (method == "POST")        response = cli->Post(path.c_str(), h, b, cType.c_str(), prog_func);
        else if (method == "PUT")    response = cli->Put(path.c_str(), h, b, cType.c_str(), prog_func);
        else if (method == "PATCH")  response = cli->Patch(path.c_str(), h, b, cType.c_str(), prog_func);
        else if (method == "DELETE") response = cli->Delete(path.c_str(), h, prog_func);
        else if (method == "HEAD")   response = cli->Head(path.c_str(), h);
        else                         response = cli->Get(path.c_str(), h, prog_func);

        if (pReq->abandoned.load()) return;

        HttpResult res;
        res.type = HttpResult::Type::COMPLETE; res.requestId = reqId;
        if (response) {
            res.status = response->status;
            res.body = std::move(response->body);
            ExtractHeaders(response->headers, res.headers);
        } else {
            res.status = 0;
            res.body = "Network Error: " + httplib::to_string(response.error());
        }
        PushResult(std::move(res));
    });

    return CreateHandle(L, info);
}

AsyncHttpProxy::AsyncHttpProxy(std::string baseUrl, sol::table defaultHeaders) : mBaseUrl(std::move(baseUrl)) {
    if (defaultHeaders != sol::lua_nil && defaultHeaders.valid()) {
        for (auto const& pair : defaultHeaders) {
            if (pair.first.is<std::string>() && pair.second.is<std::string>())
                mDefaultHeaders[pair.first.as<std::string>()] = pair.second.as<std::string>();
        }
    }
}

void AsyncHttpProxy::SetConnectTimeout(int seconds) { 
    mConnectTimeoutSeconds = seconds; 
}

void AsyncHttpProxy::SetReadTimeout(int seconds) { 
    mReadTimeoutSeconds = seconds; 
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
    for (const auto& [k, v] : outHeaders) if (ToLower(k) == "content-type") hasCT = true;

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
    std::string url = mBaseUrl + ep;
    int connectTimeout = mConnectTimeoutSeconds;
    int readTimeout = mReadTimeoutSeconds;
    bool verify = mVerifySSL;

    auto info = EnqueueTask(cb.lua_state(), MakeRef(cb), LUA_REFNIL, [=, hMap = std::move(hMap)](uint64_t reqId, std::shared_ptr<PendingRequest> pReq) {
        std::string path;
        std::unique_ptr<httplib::Client> cli;
        if (!SetupClient(url, connectTimeout, readTimeout, verify, cli, path)) throw std::runtime_error("Invalid URL");
        if (!fs::exists(filePath)) throw std::runtime_error("File not found");

        auto file_stream = std::make_shared<std::ifstream>(filePath, std::ios::binary);
        if (!file_stream || !file_stream->is_open()) throw std::runtime_error("Could not open file");

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
        for (auto const& [key, val] : hMap) {
            std::string kLower = ToLower(key);
            if (kLower == "user-agent") hasUA = true;
            if (kLower == "content-type") continue; // httplib generates this for us in PostFile
            finalH.emplace(key, val);
        }
        if (!hasUA) finalH.emplace("User-Agent", "BeamMP-Server/1.0");
        
        auto response = cli->Post(path.c_str(), finalH, regular_items, provider_items);

        HttpResult res; res.type = HttpResult::Type::COMPLETE; res.requestId = reqId;
        if (response) {
            res.status = response->status;
            res.body = std::move(response->body);
            ExtractHeaders(response->headers, res.headers);
        } else {
            res.status = 0;
            res.body = "Upload Failed: " + httplib::to_string(response.error());
        }
        PushResult(std::move(res));
    });

    return CreateHandle(cb.lua_state(), info);
}

sol::table AsyncHttpProxy::Download(std::string ep, std::string savePath, sol::function cb, sol::object prog) {
    std::string url = mBaseUrl + ep;
    int connectTimeout = mConnectTimeoutSeconds;
    int readTimeout = mReadTimeoutSeconds;

    bool verify = mVerifySSL;
    auto hMap = PrepareHeaders(sol::lua_nil);

    auto info = EnqueueTask(cb.lua_state(), MakeRef(cb), MakeRef(prog), [=, hMap = std::move(hMap)](uint64_t reqId, std::shared_ptr<PendingRequest> pReq) {
        std::string path;
        std::unique_ptr<httplib::Client> cli;
        if (!SetupClient(url, connectTimeout, readTimeout, verify, cli, path)) throw std::runtime_error("Invalid URL");
        
        std::ofstream ofs(savePath, std::ios::binary);
        if (!ofs) throw std::runtime_error("Could not open file for writing");
        
        httplib::Headers finalH;
        bool hasUA = false;
        for (auto const&[key, val] : hMap) {
            if (ToLower(key) == "user-agent") hasUA = true;
            finalH.emplace(key, val);
        }
        if (!hasUA) finalH.emplace("User-Agent", "BeamMP-Server/1.0");

        int status_code = 0; std::map<std::string, std::vector<std::string>> resHeaders;
        auto lastProg = std::chrono::steady_clock::now();
        
        auto res = cli->Get(path.c_str(), finalH, 
            [&](const httplib::Response &r) { 
                status_code = r.status; 
                ExtractHeaders(r.headers, resHeaders);
                return !g_ShuttingDown.load() && !pReq->abandoned.load(); 
            },
            [&](const char *b, size_t l) { 
                if (g_ShuttingDown.load() || pReq->abandoned.load()) return false; 
                ofs.write(b, static_cast<std::streamsize>(l)); 
                return true; 
            },
            [&](uint64_t len, uint64_t total) {
                if (pReq->progressRef != LUA_REFNIL && !pReq->abandoned.load()) {
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProg).count() > 100 || len == total) {
                        HttpResult pres; pres.type = HttpResult::Type::PROGRESS; pres.requestId = reqId;
                        pres.current = static_cast<long long>(len); pres.total = static_cast<long long>(total);
                        PushResult(std::move(pres)); lastProg = now;
                    }
                }
                return true;
            }
        );
        ofs.close();
        
        HttpResult fres; fres.type = HttpResult::Type::COMPLETE; fres.requestId = reqId;
        fres.status = status_code;
        fres.body = res ? "Success" : "Download Failed: " + httplib::to_string(res.error());
        fres.headers = resHeaders;
        PushResult(std::move(fres));
    });

    return CreateHandle(cb.lua_state(), info);
}

void AsyncHttpProxy::SetDefaultHeaders(sol::table headers) {
    mDefaultHeaders.clear();
    if (headers != sol::lua_nil && headers.valid()) {
        for (auto const& pair : headers) {
            if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
                mDefaultHeaders[pair.first.as<std::string>()] = pair.second.as<std::string>();
            }
        }
    }
}

void RegisterBindings(sol::state_view& lua) {
    lua.new_usertype<AsyncHttpProxy>("AsyncHttp", sol::no_constructor,
        "SetConnectTimeout", &AsyncHttpProxy::SetConnectTimeout,
        "SetReadTimeout", &AsyncHttpProxy::SetReadTimeout,
        "VerifySSL", &AsyncHttpProxy::VerifySSL,
        "SetDefaultHeaders", &AsyncHttpProxy::SetDefaultHeaders,
        "Get", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object h, sol::function cb) { self.Get(ep, h, cb, sol::nil); }, &AsyncHttpProxy::Get),
        "Post", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object d, sol::function cb) { self.Post(ep, d, sol::nil, cb); }, &AsyncHttpProxy::Post),
        "PostFile", sol::overload([](AsyncHttpProxy& self, std::string ep, std::string fn, std::string fp, sol::function cb) { self.PostFile(ep, fn, fp, sol::nil, cb); }, &AsyncHttpProxy::PostFile),
        "Put", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object d, sol::function cb) { self.Put(ep, d, sol::nil, cb); }, &AsyncHttpProxy::Put),
        "Patch", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::object d, sol::function cb) { self.Patch(ep, d, sol::nil, cb); }, &AsyncHttpProxy::Patch),
        "Delete", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::function cb) { self.Delete(ep, sol::nil, cb); }, &AsyncHttpProxy::Delete),
        "Head", sol::overload([](AsyncHttpProxy& self, std::string ep, sol::function cb) { self.Head(ep, sol::nil, cb); }, &AsyncHttpProxy::Head),
        "Download", sol::overload([](AsyncHttpProxy& self, std::string ep, std::string p, sol::function cb) { self.Download(ep, p, cb, sol::nil); }, &AsyncHttpProxy::Download)
    );

    lua["AsyncHttp"]["new"] = sol::overload([](std::string url) { return std::make_shared<AsyncHttpProxy>(url, sol::table(sol::lua_nil)); },[](std::string url, sol::table headers) { return std::make_shared<AsyncHttpProxy>(url, headers); }
    );
}

void Update(sol::state_view& lua) {
    lua_State* L = lua.lua_state();
    std::deque<HttpResult> toProcess;
    
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        auto it = g_Results.begin();
        while (it != g_Results.end()) {
            auto reqIt = g_PendingRequests.find(it->requestId);
            if (reqIt == g_PendingRequests.end()) {
                it = g_Results.erase(it);
            } else if (reqIt->second->L == L) {
                toProcess.push_back(std::move(*it));
                it = g_Results.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const auto& res : toProcess) {
        std::shared_ptr<PendingRequest> info;
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            if (g_PendingRequests.count(res.requestId)) info = g_PendingRequests[res.requestId];
        }

        if (!info || info->abandoned.load()) continue;

        if (res.type == HttpResult::Type::PROGRESS) {
            if (info->progressRef != LUA_REFNIL) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, info->progressRef);
                sol::protected_function prog = sol::stack::pop<sol::protected_function>(L);
                if (prog.valid()) {
                    auto r = prog(res.current, res.total);
                    if (!r.valid()) beammp_lua_errorf("AsyncHttp Progress Error: {}", sol::error(r).what());
                }
            }
        } else {
            if (info->callbackRef != LUA_REFNIL) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, info->callbackRef);
                sol::protected_function cb = sol::stack::pop<sol::protected_function>(L);
                if (cb.valid()) {
                    sol::table luaHeaders = lua.create_table();
                    for (auto const& [name, values] : res.headers) {
                        if (values.empty()) continue;

                        std::string key = ToLower(name); 

                        if (values.size() > 1 || key == "set-cookie") {
                            luaHeaders[key] = sol::as_table(values);
                        } else {
                            luaHeaders[key] = values[0];
                        }
                    }
                    auto r = cb(res.status, res.body, luaHeaders);
                    if (!r.valid()) beammp_lua_errorf("AsyncHttp Callback Error: {}", sol::error(r).what());
                }
            }
            
            if (info->callbackRef != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, info->callbackRef);
                info->callbackRef = LUA_REFNIL;
            }
            if (info->progressRef != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, info->progressRef);
                info->progressRef = LUA_REFNIL;
            }
            
            std::lock_guard<std::mutex> lock(g_Mutex);
            g_PendingRequests.erase(res.requestId);
        }
    }
}

void CleanupState(lua_State* L) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (auto it = g_PendingRequests.begin(); it != g_PendingRequests.end(); ) {
        if (it->second->L == L) {
            it->second->abandoned.store(true);
            
            if (it->second->callbackRef != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, it->second->callbackRef);
                it->second->callbackRef = LUA_REFNIL;
            }
            if (it->second->progressRef != LUA_REFNIL) {
                luaL_unref(L, LUA_REGISTRYINDEX, it->second->progressRef);
                it->second->progressRef = LUA_REFNIL;
            }
            
            it = g_PendingRequests.erase(it);
        } else {
            ++it;
        }
    }
}

void Init() { 
    g_ShuttingDown.store(false); 
    g_ThreadPool = std::make_unique<httplib::ThreadPool>(THREAD_POOL_SIZE); 
}

void Shutdown() { 
    g_ShuttingDown.store(true); 
    if (g_ThreadPool) g_ThreadPool->shutdown(); 
    g_ThreadPool.reset(); 
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_PendingRequests.clear();
    g_Results.clear();
}

} // namespace HttpAsync