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
    static std::string ServerVersion() { return "v1.20"; }
    static std::string ClientVersion() { return "v1.80"; }
    static std::string PPS() { return mPPS; }
    static void SetPPS(std::string NewPPS) { mPPS = NewPPS; }

    static inline TSettings Settings {};

private:
    static inline std::string mPPS;
    static std::unique_ptr<TConsole> mConsole;
    static inline std::mutex mShutdownHandlersMutex {};
    static inline std::deque<TShutdownHandler> mShutdownHandlers {};
};

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
    Application::Console().Write(std::string("[LUA] ") + str);
}

#define Biggest 30000
std::string Comp(std::string Data);
std::string DeComp(std::string Compressed);
