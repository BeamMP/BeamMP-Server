#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

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
        int MaxPlayers;
        bool Private;
        int MaxCars;
        bool DebugModeEnabled;
        int Port;
        std::string CustomIP;
        [[nodiscard]] bool HasCustomIP() const { return !CustomIP.empty(); }

        // new settings
        std::string ResourceFolder;
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

    static inline TSettings Settings {};

private:
    static std::unique_ptr<TConsole> mConsole;
    static inline std::mutex mShutdownHandlersMutex {};
    static inline std::vector<TShutdownHandler> mShutdownHandlers {};
};

#define warn(x) Application::Console().Write(std::string("[WARN] ") + (x))
#define error(x) Application::Console().Write(std::string("[ERROR] ") + (x))
#define info(x) Application::Console().Write(std::string("[INFO] ") + (x))
#define luaprint(x) Application::Console().Write(std::string("[LUA] ") + (x))
#define debug(x)                                                         \
    do {                                                                 \
        if (Application::Settings.DebugModeEnabled) {                    \
            Application::Console().Write(std::string("[DEBUG] ") + (x)); \
        }                                                                \
    } while (false)
