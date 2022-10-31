#pragma once

#include "TSentry.h"
extern TSentry Sentry;

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
#include <sstream>
#include <unordered_map>
#include <zlib.h>

#include <doctest/doctest.h>
#include <filesystem>
namespace fs = std::filesystem;

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
    struct TSettings {
        std::string ServerName { "BeamMP Server" };
        std::string ServerDesc { "BeamMP Default Description" };
        std::string Resource { "Resources" };
        std::string MapName { "/levels/gridmap_v2/info.json" };
        std::string Key {};
        std::string SSLKeyPath { "./.ssl/HttpServer/key.pem" };
        std::string SSLCertPath { "./.ssl/HttpServer/cert.pem" };
        bool HTTPServerEnabled { false };
        int MaxPlayers { 8 };
        bool Private { true };
        int MaxCars { 1 };
        bool DebugModeEnabled { false };
        int Port { 30814 };
        std::string CustomIP {};
        bool LogChat { true };
        bool SendErrors { true };
        bool SendErrorsMessageEnabled { true };
        int HTTPServerPort { 8080 };
        std::string HTTPServerIP { "127.0.0.1" };
        bool HTTPServerUseSSL { false };
        bool HideUpdateMessages { false };
        [[nodiscard]] bool HasCustomIP() const { return !CustomIP.empty(); }
    };

    using TShutdownHandler = std::function<void()>;

    // methods
    Application() = delete;

    // 'Handler' is called when GracefullyShutdown is called
    static void RegisterShutdownHandler(const TShutdownHandler& Handler);
    // Causes all threads to finish up and exit gracefull gracefully
    static void GracefullyShutdown();
    static TConsole& Console() { return *mConsole; }
    static std::string ServerVersionString();
    static const Version& ServerVersion() { return mVersion; }
    static uint8_t ClientMajorVersion() { return 2; }
    static std::string PPS() { return mPPS; }
    static void SetPPS(const std::string& NewPPS) { mPPS = NewPPS; }

    static TSettings Settings;

    static std::vector<std::string> GetBackendUrlsInOrder() {
        return {
            "backend.beammp.com",
            "backup1.beammp.com",
            "backup2.beammp.com"
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
        if (!mConsole) {
            mConsole = std::make_unique<TConsole>();
        }
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
    static inline std::unique_ptr<TConsole> mConsole;
    static inline std::shared_mutex mShutdownMtx {};
    static inline bool mShutdown { false };
    static inline std::mutex mShutdownHandlersMutex {};
    static inline std::deque<TShutdownHandler> mShutdownHandlers {};

    static inline Version mVersion { 3, 1, 1 };
};

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
            Sentry.AddErrorBreadcrumb((x), _file_basename, _line);                        \
        } while (false)
    #define beammp_lua_error(x)                                                               \
        do {                                                                                  \
            Application::Console().Write(_this_location + std::string("[LUA ERROR] ") + (x)); \
        } while (false)
    #define beammp_lua_warn(x)                                                               \
        do {                                                                                 \
            Application::Console().Write(_this_location + std::string("[LUA WARN] ") + (x)); \
        } while (false)
    #define luaprint(x) Application::Console().Write(_this_location + std::string("[LUA] ") + (x))
    #define beammp_debug(x)                                                                   \
        do {                                                                                  \
            if (Application::Settings.DebugModeEnabled) {                                     \
                Application::Console().Write(_this_location + std::string("[DEBUG] ") + (x)); \
            }                                                                                 \
        } while (false)
    #define beammp_event(x)                                                                   \
        do {                                                                                  \
            if (Application::Settings.DebugModeEnabled) {                                     \
                Application::Console().Write(_this_location + std::string("[EVENT] ") + (x)); \
            }                                                                                 \
        } while (false)
    // trace() is a debug-build debug()
    #if defined(DEBUG)
        #define beammp_trace(x)                                                                   \
            do {                                                                                  \
                if (Application::Settings.DebugModeEnabled) {                                     \
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

#endif // DOCTEST_CONFIG_DISABLE

#if defined(DEBUG)
    #define SU_RAW SSU_UNRAW
#else
    #define SU_RAW RAWIFY(SSU_UNRAW)
    #define _this_location (ThreadName())
#endif

// clang-format on

void LogChatMessage(const std::string& name, int id, const std::string& msg);

#define Biggest 30000

template <typename T>
inline T Comp(const T& Data) {
    std::array<char, Biggest> C {};
    // obsolete
    C.fill(0);
    z_stream defstream;
    defstream.zalloc = nullptr;
    defstream.zfree = nullptr;
    defstream.opaque = nullptr;
    defstream.avail_in = uInt(Data.size());
    defstream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(&Data[0]));
    defstream.avail_out = Biggest;
    defstream.next_out = reinterpret_cast<Bytef*>(C.data());
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_SYNC_FLUSH);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);
    size_t TotalOut = defstream.total_out;
    T Ret;
    Ret.resize(TotalOut);
    std::fill(Ret.begin(), Ret.end(), 0);
    std::copy_n(C.begin(), TotalOut, Ret.begin());
    return Ret;
}

template <typename T>
inline T DeComp(const T& Compressed) {
    std::array<char, Biggest> C {};
    // not needed
    C.fill(0);
    z_stream infstream;
    infstream.zalloc = nullptr;
    infstream.zfree = nullptr;
    infstream.opaque = nullptr;
    infstream.avail_in = Biggest;
    infstream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(&Compressed[0]));
    infstream.avail_out = Biggest;
    infstream.next_out = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(C.data()));
    inflateInit(&infstream);
    inflate(&infstream, Z_SYNC_FLUSH);
    inflate(&infstream, Z_FINISH);
    inflateEnd(&infstream);
    size_t TotalOut = infstream.total_out;
    T Ret;
    Ret.resize(TotalOut);
    std::fill(Ret.begin(), Ret.end(), 0);
    std::copy_n(C.begin(), TotalOut, Ret.begin());
    return Ret;
}

std::string GetPlatformAgnosticErrorString();
#define S_DSN SU_RAW
