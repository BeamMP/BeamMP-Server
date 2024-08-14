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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <unordered_map>
#include <zlib.h>

#include <doctest/doctest.h>
#include <filesystem>
namespace fs = std::filesystem;

#include "Settings.h"
#include "TConsole.h"

struct Version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    Version(uint8_t major, uint8_t minor, uint8_t patch);
    Version(const std::array<uint8_t, 3>& v);
    std::string AsString();
};

template <typename T>
using SparseArray = std::unordered_map<size_t, T>;

// static class handling application start, shutdown, etc.
// yes, static classes, singletons, globals are all pretty
// bad idioms. In this case we need a central way to access
// stuff like graceful shutdown, global settings (its in the name),
// etc.
class Application final {
public:
    // types

    using TShutdownHandler = std::function<void()>;

    // methods
    Application() = delete;

    // 'Handler' is called when GracefullyShutdown is called
    static void RegisterShutdownHandler(const TShutdownHandler& Handler);
    // Causes all threads to finish up and exit gracefull gracefully
    static void GracefullyShutdown();
    static TConsole& Console() { return mConsole; }
    static std::string ServerVersionString();
    static const Version& ServerVersion() { return mVersion; }
    static uint8_t ClientMajorVersion() { return 2; }
    static std::string PPS() { return mPPS; }
    static void SetPPS(const std::string& NewPPS) { mPPS = NewPPS; }

    static inline struct Settings Settings { };

    static std::vector<std::string> GetBackendUrlsInOrder() {
        return {
            "backend.beammp.com",
        };
    }

    static std::string GetBackendUrlForAuth() { return "auth.beammp.com"; }
    static std::string GetBackendUrlForSocketIO() { return "https://backend.beammp.com"; }
    static void CheckForUpdates();
    static std::array<uint8_t, 3> VersionStrToInts(const std::string& str);
    static bool IsOutdated(const Version& Current, const Version& Newest);
    static bool IsShuttingDown();
    static void SleepSafeSeconds(size_t Seconds);

    static void InitializeConsole() {
        mConsole.InitializeCommandline();
    }

    enum class Status {
        Starting,
        Good,
        Bad,
        ShuttingDown,
        Shutdown,
    };

    using SystemStatusMap = std::unordered_map<std::string /* system name */, Status /* status */>;

    static const SystemStatusMap& GetSubsystemStatuses() {
        std::unique_lock Lock(mSystemStatusMapMutex);
        return mSystemStatusMap;
    }

    static void SetSubsystemStatus(const std::string& Subsystem, Status status);

private:
    static void SetShutdown(bool Val);

    static inline SystemStatusMap mSystemStatusMap {};
    static inline std::mutex mSystemStatusMapMutex {};
    static inline std::string mPPS;
    static inline TConsole mConsole;
    static inline std::shared_mutex mShutdownMtx {};
    static inline bool mShutdown { false };
    static inline std::mutex mShutdownHandlersMutex {};
    static inline std::deque<TShutdownHandler> mShutdownHandlers {};

    static inline Version mVersion { 3, 5, 0 };
};

void SplitString(std::string const& str, const char delim, std::vector<std::string>& out);

std::string ThreadName(bool DebugModeOverride = false);
void RegisterThread(const std::string& str);
#define RegisterThreadAuto() RegisterThread(__func__)

#define KB 1024llu
#define MB (KB * 1024llu)
#define GB (MB * 1024llu)
#define SSU_UNRAW SECRET_SENTRY_URL

#define _file_basename std::filesystem::path(__FILE__).filename().string()
#define _line std::to_string(__LINE__)
#define _in_lambda (std::string(__func__) == "operator()")

// for those times when you just need to ignore something :^)
// explicity disables a [[nodiscard]] warning
#define beammp_ignore(x) (void)x

// clang-format off
#ifdef DOCTEST_CONFIG_DISABLE

    // we would like the full function signature 'void a::foo() const'
    // on windows this is __FUNCSIG__, on GCC it's __PRETTY_FUNCTION__,
    // feel free to add more
    #if defined(WIN32)
        #define _function_name std::string(__FUNCSIG__)
    #elif defined(__unix) || defined(__unix__)
        #define _function_name std::string(__PRETTY_FUNCTION__)
    #else
        #define _function_name std::string(__func__)
    #endif
    
    #ifndef NDEBUG
        #define DEBUG
    #endif
    
    #if defined(DEBUG)
        
        // if this is defined, we will show the full function signature infront of
        // each info/debug/warn... call instead of the 'filename:line' format.
        #if defined(BMP_FULL_FUNCTION_NAMES)
            #define _this_location (ThreadName() + _function_name + " ")
        #else
            #define _this_location (ThreadName() + _file_basename + ":" + _line + " ")
        #endif

    #endif // defined(DEBUG)
    
    #define beammp_warn(x) Application::Console().Write(_this_location + std::string("[WARN] ") + (x))
    #define beammp_info(x) Application::Console().Write(_this_location + std::string("[INFO] ") + (x))
    #define beammp_error(x)                                                               \
        do {                                                                              \
            Application::Console().Write(_this_location + std::string("[ERROR] ") + (x)); \
        } while (false)
    #define beammp_lua_error(x)                                                               \
        do {                                                                                  \
            Application::Console().Write(_this_location + std::string("[LUA ERROR] ") + (x)); \
        } while (false)
    #define beammp_lua_log(level, plugin, x)                                                               \
        do {                                                                                 \
            Application::Console().Write(_this_location + fmt::format("[{}] [{}] ", plugin, level) + (x)); \
        } while (false)
    #define beammp_lua_warn(x)                                                               \
        do {                                                                                 \
            Application::Console().Write(_this_location + std::string("[LUA WARN] ") + (x)); \
        } while (false)
    #define luaprint(x) Application::Console().Write(_this_location + std::string("[LUA] ") + (x))
    #define beammp_debug(x)                                                                   \
        do {                                                                                  \
            if (Application::Settings.getAsBool(Settings::Key::General_Debug)) {                                     \
                Application::Console().Write(_this_location + std::string("[DEBUG] ") + (x)); \
            }                                                                                 \
        } while (false)
    #define beammp_event(x)                                                                   \
        do {                                                                                  \
            if (Application::Settings.getAsBool(Settings::Key::General_Debug)) {                                     \
                Application::Console().Write(_this_location + std::string("[EVENT] ") + (x)); \
            }                                                                                 \
        } while (false)
    // trace() is a debug-build debug()
    #if defined(DEBUG)
        #define beammp_trace(x)                                                                   \
            do {                                                                                  \
                if (Application::Settings.getAsBool(Settings::Key::General_Debug)) {                                     \
                    Application::Console().Write(_this_location + std::string("[TRACE] ") + (x)); \
                }                                                                                 \
            } while (false)
    #else
        #define beammp_trace(x)
    #endif // defined(DEBUG)
    
    #define beammp_errorf(...) beammp_error(fmt::format(__VA_ARGS__))
    #define beammp_infof(...) beammp_info(fmt::format(__VA_ARGS__))
    #define beammp_debugf(...) beammp_debug(fmt::format(__VA_ARGS__))
    #define beammp_warnf(...) beammp_warn(fmt::format(__VA_ARGS__))
    #define beammp_tracef(...) beammp_trace(fmt::format(__VA_ARGS__))
    #define beammp_lua_errorf(...) beammp_lua_error(fmt::format(__VA_ARGS__))
    #define beammp_lua_warnf(...) beammp_lua_warn(fmt::format(__VA_ARGS__))

#else // DOCTEST_CONFIG_DISABLE

    #define beammp_error(x) /* x */
    #define beammp_lua_error(x) /* x */
    #define beammp_warn(x) /* x */
    #define beammp_lua_warn(x) /* x */
    #define beammp_info(x) /* x */
    #define beammp_event(x) /* x */
    #define beammp_debug(x) /* x */
    #define beammp_trace(x) /* x */
    #define luaprint(x) /* x */
    #define beammp_errorf(...) beammp_error(fmt::format(__VA_ARGS__))
    #define beammp_infof(...) beammp_info(fmt::format(__VA_ARGS__))
    #define beammp_warnf(...) beammp_warn(fmt::format(__VA_ARGS__))
    #define beammp_debugf(...) beammp_debug(fmt::format(__VA_ARGS__))
    #define beammp_tracef(...) beammp_trace(fmt::format(__VA_ARGS__))
    #define beammp_lua_errorf(...) beammp_lua_error(fmt::format(__VA_ARGS__))
    #define beammp_lua_warnf(...) beammp_lua_warn(fmt::format(__VA_ARGS__))
    #define beammp_lua_log(level, plugin, x) /* x */

#endif // DOCTEST_CONFIG_DISABLE

#if defined(DEBUG)
    #define SU_RAW SSU_UNRAW
#else
    #define SU_RAW RAWIFY(SSU_UNRAW)
    #define _this_location (ThreadName())
#endif

// clang-format on

void LogChatMessage(const std::string& name, int id, const std::string& msg);

std::vector<uint8_t> Comp(std::span<const uint8_t> input);
std::vector<uint8_t> DeComp(std::span<const uint8_t> input);

std::string GetPlatformAgnosticErrorString();
#define S_DSN SU_RAW
