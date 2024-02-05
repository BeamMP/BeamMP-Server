#pragma once

#include "Common.h"
#include "FileWatcher.h"
#include "HashMap.h"
#include "Plugin.h"
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/post.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/thread/scoped_thread.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <sol/forward.hpp>
#include <sol/variadic_args.hpp>
#include <spdlog/logger.h>

#include <sol/sol.hpp>

struct Timer {
    Timer(boost::asio::deadline_timer&& timer_, long interval_)
        : timer(std::move(timer_))
        , interval(interval_) { }
    boost::asio::deadline_timer timer;
    boost::posix_time::milliseconds interval;
};

class LuaPlugin : public Plugin {
public:
    /// Declare a new plugin with the path.
    /// Loading of any files only happens on LuaPlugin::initialize().
    LuaPlugin(const std::string& path);
    /// Shuts down lua thread, may hang if there is still work to be done.
    ~LuaPlugin();

    template <typename FnT>
    void register_function(const std::string& table, const std::string& identifier, const FnT& func) {
        boost::asio::post(m_io, [this, table, identifier, func] {
            if (!m_state.globals()[table].valid()) {
                m_state.globals().create_named(table);
            }
            if (m_state.globals()[table][identifier].valid()) {
                beammp_errorf("Global '{}.{}' already exists and could not be injected as function.", table, identifier);
            } else {
                m_state.globals()[table][identifier] = func;
            }
        });
    }

private:
    /// Initializes the error handlers for panic and exceptions.
    Error initialize_error_handlers();
    /// Initializes / loads base packages and libraries.
    Error initialize_libraries();
    /// Loads main file of the plugin.
    Error load_files();
    /// Overrides functions such as `print()`
    Error initialize_overrides();
    /// Fixes lua's package.path and package.cpath to understand our own file structure better.
    Error fix_lua_paths();

    /// Loads an extension. Call this from the lua thread.
    /// This function cannot fail, as it reports errors to the user.
    void load_extension(const std::filesystem::path& file, const std::string& ext_name);
    /// Loads all extension from the folder, using the base as a prefix.
    /// This function is recursive.
    /// Returns the amount of extensions found. This function cannot fail.
    size_t load_extensions(const std::filesystem::path& extensions_folder, const std::string& base = "");

    /// Entry point for the lua plugin's thread.
    void thread_main();

    // Plugin interface
public:
    /// Initializes the Lua Plugin, loads file(s), starts executing code.
    virtual Error initialize() override;
    // TODO cleanup
    virtual Error cleanup() override;
    // TODO reload
    virtual Error reload() override;
    /// Name of this lua plugin (the base name of the folder).
    virtual std::string name() const override;
    /// Path to the folder containing this lua plugin.
    virtual std::filesystem::path path() const override;
    /// Dispatches the event to the thread which runs all lua.
    virtual std::shared_future<std::optional<Value>> handle_event(const std::string& event_name, const std::shared_ptr<Value>& args) override;
    /// Returns the memory usage of this thread, updated at the slowest every 5 seconds.
    virtual size_t memory_usage() const override;

private:
    /// Path to the plugin's root folder.
    std::filesystem::path m_path;
    /// Thread where all lua work must happen. Started within the constructor but is blocked until LuaPlugin::initialize is called
    boost::scoped_thread<> m_thread;
    /// This asio context schedules all tasks. It's run in the m_thread thread.
    boost::asio::io_context m_io;

    /// Event handlers which are legacy-style (by name)
    HashMap<std::string, std::string> m_event_handlers_named {};
    /// Event handlers which are functions (v4 style)
    HashMap<std::string, sol::protected_function> m_event_handlers {};

    /// Main (and usually only) lua state of this plugin.
    /// ONLY access this from the m_thread thread.
    sol::state m_state;
    /// Whether the lua thread should shutdown. Triggered by the LuaPlugin::~LuaPlugin dtor.
    boost::synchronized_value<bool> m_shutdown { false };
    /// Current memory usage. Cached to avoid having to synchronize access to the lua state.
    boost::synchronized_value<size_t> m_memory { 0 };
    /// Hash map of all event handlers in this state.
    // HashMap<std::string, sol::protected_function> m_event_handlers {};
    SynchronizedHashMap<std::string, std::filesystem::path> m_known_extensions {};

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work_guard = boost::asio::make_work_guard(m_io);

    /// Iteration options to be used whenever iterating over a directory in this class.
    static inline auto s_directory_iteration_options = std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied;

    FileWatcher m_extensions_watcher { 2 };

    std::vector<std::shared_ptr<Timer>> m_timers {};

    std::shared_ptr<Timer> make_timer(size_t ms);
    void cancel_timer(const std::shared_ptr<Timer>& timer);

    // Lua API
    /// Override for lua's base.print().
    /// Dumps tables, arrays, etc. properly.
    void l_print(const sol::variadic_args&);

    std::shared_ptr<Timer> l_mp_schedule_call_repeat(size_t ms, const sol::function& fn, sol::variadic_args args);
    void l_mp_schedule_call_helper(const boost::system::error_code& err, std::shared_ptr<Timer> timer, const sol::function& fn, std::shared_ptr<ValueTuple> args);
    void l_mp_schedule_call_once(size_t ms, const sol::function& fn, sol::variadic_args args);

    std::string print_impl(const sol::variadic_args&);
};
