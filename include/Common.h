#pragma once

#include <atomic>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>

#include "Compat.h"

#include "TConsole.h"

struct Version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    Version(uint8_t major, uint8_t minor, uint8_t patch);
    std::string AsString();
};

// static class handling application start, shutdown, etc.
// yes, static classes, singletons, globals are all pretty
// bad idioms. In this case we need a central way to access
// stuff like graceful shutdown, global settings (its in the name),
// etc.
class Application final {
public:
    // types
    struct TSettings {
        TSettings() noexcept
            : ServerName("BeamMP Server")
            , ServerDesc("BeamMP Default Description")
            , Resource("Resources")
            , MapName("/levels/gridmap_v2/info.json")
            , MaxPlayers(10)
            , Private(false)
            , MaxCars(1)
            , DebugModeEnabled(false)
            , Port(30814) { }
        std::string ServerName;
        std::string ServerDesc;
        std::string Resource;
        std::string MapName;
        std::string Key;
        int MaxPlayers;
        bool Private;
        int MaxCars;
        bool DebugModeEnabled;
        int Port;
        std::string CustomIP;
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
    static std::string ClientVersionString() { return "2.0"; }
    static std::string PPS() { return mPPS; }
    static void SetPPS(std::string NewPPS) { mPPS = NewPPS; }

    static inline TSettings Settings {};

    static std::string GetBackendUrlForAuth() { return "auth.beammp.com"; }
    static std::string GetBackendHostname() { return "backend.beammp.com"; }
    static std::string GetBackendUrlForSocketIO() { return "https://backend.beammp.com"; }

private:
    static inline std::string mPPS;
    static std::unique_ptr<TConsole> mConsole;
    static inline std::mutex mShutdownHandlersMutex {};
    static inline std::deque<TShutdownHandler> mShutdownHandlers {};

    static inline Version mVersion { 2, 3, 0 };
};

std::string ThreadName();
void RegisterThread(const std::string str);
#define RegisterThreadAuto() RegisterThread(__func__)

#define KB 1024
#define MB (KB * 1024)

#define _file_basename std::filesystem::path(__FILE__).filename().string()
#define _line std::to_string(__LINE__)
#define _in_lambda (std::string(__func__) == "operator()")

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

#if defined(DEBUG)

// if this is defined, we will show the full function signature infront of
// each info/debug/warn... call instead of the 'filename:line' format.
#if defined(BMP_FULL_FUNCTION_NAMES)
#define _this_location (ThreadName() + _function_name + " ")
#else
#define _this_location (ThreadName() + _file_basename + ":" + _line + " ")
#endif

#else // !defined(DEBUG)

#define _this_location (ThreadName())

#endif // defined(DEBUG)

#define beammp_warn(x) Application::Console().Write(_this_location + std::string("[WARN] ") + (x))
#define beammp_info(x) Application::Console().Write(_this_location + std::string("[INFO] ") + (x))
#define beammp_error(x) Application::Console().Write(_this_location + std::string("[ERROR] ") + (x))
#define beammp_lua_error(x) Application::Console().Write(_this_location + std::string("[LUA ERROR] ") + (x))
#define luaprint(x) Application::Console().Write(_this_location + std::string("[LUA] ") + (x))
#define beammp_debug(x)                                                                   \
    do {                                                                                  \
        if (Application::Settings.DebugModeEnabled) {                                     \
            Application::Console().Write(_this_location + std::string("[DEBUG] ") + (x)); \
        }                                                                                 \
    } while (false)

#define Biggest 30000
std::string Comp(std::string Data);
std::string DeComp(std::string Compressed);

std::string GetPlatformAgnosticErrorString();
