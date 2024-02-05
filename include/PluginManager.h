#pragma once

#include "HashMap.h"
#include "Plugin.h"

/// The PluginManager class manages all plugins, specifically their lifetime,
/// events and memory.
class PluginManager {
public:
    /// Iterates through all plugins, ask them for their usage, take the sum.
    size_t memory_usage() const {
        size_t total = 0;
        auto plugins = m_plugins.synchronize();
        for (const auto& [name, plugin] : *plugins) {
            (void)name; // name ignored
            total += plugin->memory_usage();
        }
        return total;
    }

    /// Triggers (sends) the event to all plugins and gathers the results as futures.
    ///
    /// PLEASE be aware that some plugins' handlers may take a while to handle the event,
    /// so try not to wait on these futures without a timeout.
    ///
    /// This function should not block.
    std::vector<std::shared_future<std::optional<Value>>> trigger_event(const std::string& event_name, const std::shared_ptr<Value>& args) {
        // results will go in here
        std::vector<std::shared_future<std::optional<Value>>> results;
        // synchronize practically grabs a lock to the mutex, this is (as the name suggests)
        // a synchronization point. technically, it could dead-lock if something that is called
        // in this locked context tries to lock the m_plugins mutex.
        // Plugin::handle_event should NEVER do much else than dispatch the event to the
        // plugin's main thread, so this really cannot happen.
        // that said, if you end up here with gdb, make sure it doesn't ;)
        auto plugins = m_plugins.synchronize();
        // allocate as many as we could possibly have, to avoid realloc()'s
        results.reserve(plugins->size());
        for (const auto& [name, plugin] : *plugins) {
            (void)name; // ignore the name
            // propagates the event to the plugin, this returns a future
            // we assume that at this point no plugin-specific code has executed
            auto maybe_result = plugin->handle_event(event_name, args);
            // if the plugin had no handler, this result has no value, and we can ignore it
            results.push_back(maybe_result);
        }
        return results;
    }

    /// Adds the plugin, calls Plugin::initialize(), and so on
    [[nodiscard]] Error add_plugin(Plugin::Pointer&& plugin) {
        auto plugins = m_plugins.synchronize();
        if (plugins->contains(plugin->name())) {
            return Error("Plugin with the name '{}' already exists, refusing to replace it.", plugin->name());
        } else {
            auto [iter, b] = plugins->insert({ plugin->name(), std::move(plugin) });
            (void)b; // ignore b
            auto err = iter->second->initialize();
            if (err) {
                return err;
            }
            return {};
        }
    }

private:
    /// All plugins as pointers to allow inheritance.
    SynchronizedHashMap<std::string, Plugin::Pointer> m_plugins;
};

