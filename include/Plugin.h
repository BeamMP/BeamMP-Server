#pragma once

#include "Error.h"
#include "Value.h"
#include <filesystem>
#include <future>
#include <utility>
#include <variant>

/// The Plugin class is an abstract interface for any plugin.
///
/// A plugin must itself figure out how to deal with events, load itself,
/// and must react quickly and appropriately to any incoming events or function calls.
/// A plugin may *not* ever block a calling thread unless explicitly marked with
/// "this may block the caller" or similar.
class Plugin {
public:
    /// Self-managing pointer type of this plugin.
    using Pointer = std::unique_ptr<Plugin>;
    /// Allocates a Plugin of the specific derived plugin type.
    template <typename T, typename... Args>
    static Pointer make_pointer(Args&&... args) {
        return std::unique_ptr<Plugin>(new T(std::forward<Args>(args)...));
    }

    /// Default constructor to enable derived classes to default-construct.
    Plugin() = default;

    /// Plugin is not copyable.
    Plugin(const Plugin&) = delete;
    /// Plugin is not copyable.
    Plugin& operator=(const Plugin&) = delete;
    /// Plugin is movable.
    Plugin(Plugin&&) = default;
    /// Plugin is movable.
    Plugin& operator=(Plugin&&) = default;

    /// Default destructor but virtual, to make the compiler happy.
    virtual ~Plugin() = default;

    /// Called when the plugin should initialize its state.
    /// This may block the caller.
    virtual Error initialize() = 0;
    /// Called when the plugin should tear down and clean up its state.
    /// This may block the caller.
    virtual Error cleanup() = 0;
    /// Called when the plugin should be reloaded. Usually it's a good idea
    /// to notify the plugin's code, call cleanup(), etc. internally.
    virtual Error reload() = 0;

    /// Returns the name of the plugin.
    virtual std::string name() const = 0;
    /// Returns the path to the plugin - this can either be the folder in which
    /// the plugin's files live, or the plugin itself if it's a single file.
    /// The exact format of what this returns (directory/file) is implementation defined.
    virtual std::filesystem::path path() const = 0;

    /// Instructs the plugin to handle the given event, with the given arguments.
    /// Returns a future with a result if this event will be handled by the plugin, otherwise must return
    /// std::nullopt.
    virtual std::shared_future<std::vector<Value>> handle_event(const std::string& event_name, const std::shared_ptr<Value>& args) = 0;

    /// Returns how much memory this state thinks it uses.
    ///
    /// This value is difficult to calculate for some use-cases, but a rough ballpark
    /// should be returned regardless.
    virtual size_t memory_usage() const = 0;
};
