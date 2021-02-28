#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

#include "TConsole.h"

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
            : DebugModeEnabled(true) { }
        std::string ServerName;
        std::string ServerDesc;
        std::string Resource;
        std::string MapName;
        std::string Key;
        int MaxPlayers {};
        bool Private {};
        int MaxCars {};
        bool DebugModeEnabled;
        int Port {};
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
    static std::string ServerVersion() { return "1.20"; }
    static std::string ClientVersion() { return "1.80"; }
    static std::string PPS() { return mPPS; }
    static void SetPPS(std::string NewPPS) { mPPS = NewPPS; }

    static inline TSettings Settings {};

private:
    static inline std::string mPPS;
    static std::unique_ptr<TConsole> mConsole;
    static inline std::mutex mShutdownHandlersMutex {};
    static inline std::deque<TShutdownHandler> mShutdownHandlers {};
};

#define KB 1024
#define MB (KB * 1024)

#ifndef DEBUG
static inline void warn(const std::string& str) {
    Application::Console().Write(std::string("[WARN] ") + str);
}
static inline void error(const std::string& str) {
    Application::Console().Write(std::string("[ERROR] ") + str);
}
static inline void info(const std::string& str) {
    Application::Console().Write(std::string("[INFO] ") + str);
}
static inline void debug(const std::string& str) {
    if (Application::Settings.DebugModeEnabled) {
        Application::Console().Write(std::string("[DEBUG] ") + str);
    }
}
static inline void luaprint(const std::string& str) {
    Application::Console().WriteRaw(str);
}
#else // DEBUG

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

// if this is defined, we will show the full function signature infront of
// each info/debug/warn... call instead of the 'filename:line' format.
#if defined(BMP_FULL_FUNCTION_NAMES)
#define _this_location (_function_name)
#else
#define _this_location (_file_basename + ":" + _line)
#endif

#define warn(x) Application::Console().Write(_this_location + std::string(" [WARN] ") + (x))
#define info(x) Application::Console().Write(_this_location + std::string(" [INFO] ") + (x))
#define error(x) Application::Console().Write(_this_location + std::string(" [ERROR] ") + (x))
#define luaprint(x) Application::Console().Write(_this_location + std::string(" [LUA] ") + (x))
#define debug(x)                                                                           \
    do {                                                                                   \
        if (Application::Settings.DebugModeEnabled) {                                      \
            Application::Console().Write(_this_location + std::string(" [DEBUG] ") + (x)); \
        }                                                                                  \
    } while (false)
#endif // DEBUG

#define Biggest 30000
std::string Comp(std::string Data);
std::string DeComp(std::string Compressed);
