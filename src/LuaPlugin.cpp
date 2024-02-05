#include "LuaPlugin.h"
#include "Common.h"
#include "LuaAPI.h"
#include "Value.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/post.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/date_time/posix_time/posix_time_config.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/thread/exceptions.hpp>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <functional>
#include <lauxlib.h>
#include <lua.h>
#include <optional>
#include <regex>
#include <sol/forward.hpp>
#include <sol/sol.hpp>
#include <sol/types.hpp>
#include <sol/variadic_args.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>

static int lua_panic_handler(lua_State* state) {
    sol::state_view view(state);
    sol::state new_state {};
    luaL_traceback(state, new_state.lua_state(), nullptr, 1);
    auto traceback = new_state.get<std::string>(-1);
    beammp_errorf("Lua panic (unclear in which plugin): {}", traceback);
    return 1;
}

#define beammp_lua_debugf(...) beammp_debugf("[{}] {}", name(), fmt::format(__VA_ARGS__))
#define beammp_lua_infof(...) beammp_infof("[{}] {}", name(), fmt::format(__VA_ARGS__))
#define beammp_lua_warnf(...) beammp_warnf("[{}] {}", name(), fmt::format(__VA_ARGS__))
#define beammp_lua_errorf(...) beammp_errorf("[{}] {}", name(), fmt::format(__VA_ARGS__))
#define beammp_lua_tracef(...) beammp_tracef("[{}] {}", name(), fmt::format(__VA_ARGS__))

static constexpr const char* ERR_HANDLER = "__beammp_lua_error_handler";

/// Checks whether the supplied name is a valid lua identifier (mostly).
static inline bool check_name_validity(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (std::isdigit(name.at(0))) {
        return false;
    }
    for (const char c : name) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

LuaPlugin::LuaPlugin(const std::string& path)
    : m_path(path) {
    m_state = sol::state(lua_panic_handler);
    m_thread = boost::scoped_thread<> { &LuaPlugin::thread_main, this };
}

LuaPlugin::~LuaPlugin() {
    for (auto& timer : m_timers) {
        timer->timer.cancel();
    }
    // work guard reset means that we allow all work to be finished before exit
    m_work_guard.reset();
    // setting this flag signals the thread to shut down
    *m_shutdown = true;
}

Error LuaPlugin::initialize_error_handlers() {
    m_state.set_exception_handler([](lua_State* state, sol::optional<const std::exception&>, auto err) -> int {
        beammp_errorf("Error (unclear in which plugin): {}", err); // TODO: wtf?
        return sol::stack::push(state, err);
    });
    m_state.globals()[ERR_HANDLER] = [this](const std::string& error) {
        beammp_lua_errorf("Error: {}", error);
        return error;
    };
    return {};
}

Error LuaPlugin::initialize_libraries() {
    m_state.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::os,
        sol::lib::math,
        sol::lib::table,
        sol::lib::debug,
        sol::lib::bit32,
        sol::lib::io);

    auto& glob = m_state.globals();

    glob.create_named("MP");
    glob["MP"]["GetExtensions"] = [this]() -> sol::table {
            auto table = m_state.create_table();
            auto extensions = m_known_extensions.synchronize();
            for (const auto& [ext, path] : *extensions) {
                (void)path;
                table[ext] = m_state.globals()[ext];
            }
            return table; };
    glob["MP"]["GetStateMemoryUsage"] = [this]() { return size_t(m_state.memory_used()); };
    glob["MP"]["GetPluginMemoryUsage"] = [this] { return memory_usage(); };
    glob["MP"]["LogError"] = [this](const sol::variadic_args& args) {
        auto result = print_impl(args);
        beammp_lua_errorf("[out] {}", result);
    };
    glob["MP"]["LogWarn"] = [this](const sol::variadic_args& args) {
        auto result = print_impl(args);
        beammp_lua_warnf("[out] {}", result);
    };
    glob["MP"]["LogInfo"] = [this](const sol::variadic_args& args) {
        auto result = print_impl(args);
        beammp_lua_infof("[out] {}", result);
    };
    glob["MP"]["LogDebug"] = [this](const sol::variadic_args& args) {
        auto result = print_impl(args);
        beammp_lua_debugf("[out] {}", result);
    };
    glob["MP"]["GetPluginPath"] = [this] {
        return std::filesystem::absolute(m_path).string();
    };
    glob["MP"]["RegisterEvent"] = [this](const std::string& event_name, const sol::object& handler) {
        if (handler.get_type() == sol::type::string) {
            m_event_handlers_named[event_name].push_back(handler.as<std::string>());
        } else if (handler.get_type() == sol::type::function) {
            auto fn = handler.as<sol::protected_function>();
            fn.set_error_handler(m_state.globals()[ERR_HANDLER]);
            m_event_handlers[event_name].push_back(fn);
        } else {
            beammp_lua_errorf("Invalid call to MP.RegisterEvent for event '{}': Expected string or function as second argument", event_name);
        }
    };
    glob["MP"]["Post"] = [this](const sol::protected_function& fn) {
        boost::asio::post(m_io, [fn] {
            fn();
        });
    };
    glob["MP"]["TriggerLocalEvent"] = [this](const std::string& event_name, sol::variadic_args args) -> sol::table {
        std::vector<sol::object> args_vec(args.begin(), args.end());
        ValueTuple values {};
        values.reserve(args_vec.size());
        for (const auto& obj : args_vec) {
            auto res = sol_obj_to_value(obj);
            if (res) [[likely]] {
                values.emplace_back(res.move());
            } else {
                beammp_lua_errorf("Can't serialize an argument across boundaries (for passing to a MP.TriggerLocalEvent): {}", res.error);
                values.emplace_back(Null {});
            }
        }
        auto results = handle_event(event_name, std::make_shared<Value>(std::move(values)));
        auto result = m_state.create_table();
        result["__INTERNAL"] = results;
        // waits for a specific number of milliseconds for a result to be available, returns false if timed out, true if ready.
        result["WaitForMS"] = [](const sol::object& self, int milliseconds) {
            std::shared_future<std::vector<Value>> future = self.as<sol::table>()["__INTERNAL"];
            auto status = future.wait_for(std::chrono::milliseconds(milliseconds));
            return status == std::future_status::ready;
        };
        // waits indefinitely for all results to be available
        result["Wait"] = [](const sol::table& self) {
            std::shared_future<std::vector<Value>> future = self["__INTERNAL"];
            future.wait();
        };
        // waits indefinitely for all results to be available, then returns them
        result["Results"] = [this](const sol::table& self) -> sol::table {
            std::shared_future<std::vector<Value>> future = self["__INTERNAL"];
            future.wait();
            Value results = future.get();
            auto lua_res = boost::apply_visitor(ValueToLuaVisitor(m_state), results);
            return lua_res;
        };
        return result;
    };
    glob["MP"]["ScheduleCallRepeat"] = [this](size_t ms, sol::function fn, sol::variadic_args args) {
        return l_mp_schedule_call_repeat(ms, fn, args);
    };
    glob["MP"]["ScheduleCallOnce"] = [this](size_t ms, sol::function fn, sol::variadic_args args) {
        l_mp_schedule_call_once(ms, fn, args);
    };
    glob["MP"]["CancelScheduledCall"] = [this](std::shared_ptr<Timer>& timer) {
        // this has to be post()-ed, otherwise the call will not cancel (not sure why)
        boost::asio::post(m_io, [this, timer] {
            if (!timer) {
                beammp_lua_errorf("MP.cancel_scheduled_call: timer already cancelled");
                return;
            }
            beammp_lua_debugf("Cancelling timer");
            cancel_timer(timer);
        });
        // release the lua's reference to this timer
        timer.reset();
    };

    glob["MP"]["GetOSName"] = &LuaAPI::MP::GetOSName;
    // glob["MP"]["GetTimeMS"] = &LuaAPI::MP::GetTimeMS;
    // glob["MP"]["GetTimeS"] = &LuaAPI::MP::GetTimeS;

    glob.create_named("Util");
    glob["Util"]["JsonEncode"] = &LuaAPI::Util::JsonEncode;
    glob["Util"]["JsonDiff"] = &LuaAPI::Util::JsonDiff;
    glob["Util"]["JsonDiffApply"] = &LuaAPI::Util::JsonDiffApply;
    glob["Util"]["JsonPrettify"] = &LuaAPI::Util::JsonPrettify;
    glob["Util"]["JsonMinify"] = &LuaAPI::Util::JsonMinify;
    glob["Util"]["JsonFlatten"] = &LuaAPI::Util::JsonFlatten;
    glob["Util"]["JsonUnflatten"] = &LuaAPI::Util::JsonUnflatten;
    glob["Util"]["JsonDecode"] = &LuaAPI::Util::JsonDecode;

    glob.create_named("FS");
    glob["FS"]["Exists"] = &LuaAPI::FS::Exists;
    glob["FS"]["CreateDirectory"] = &LuaAPI::FS::CreateDirectory;
    glob["FS"]["ConcatPaths"] = &LuaAPI::FS::ConcatPaths;
    glob["FS"]["IsFile"] = &LuaAPI::FS::IsFile;
    glob["FS"]["Remove"] = &LuaAPI::FS::Remove;
    glob["FS"]["GetFilename"] = &LuaAPI::FS::GetFilename;
    glob["FS"]["IsDirectory"] = &LuaAPI::FS::IsDirectory;
    glob["FS"]["GetExtensinon"] = &LuaAPI::FS::GetExtension;
    glob["FS"]["GetParentFolder"] = &LuaAPI::FS::GetParentFolder;
    glob["FS"]["Copy"] = &LuaAPI::FS::Copy;
    glob["FS"]["Rename"] = &LuaAPI::FS::Rename;
    glob["FS"]["ListFiles"] = &LuaAPI::FS::ListFiles;

    glob["FS"]["PathSep"] = fmt::format("{}", char(std::filesystem::path::preferred_separator));
    return {};
}

Error LuaPlugin::load_files() {
    // 1. look for main.lua, run that
    // 2. look for extensions in extensions/, load those.
    //    make those globals based on the filename
    // 3. call onInit by name (global)
    auto extensions_folder = m_path / "extensions";
    if (std::filesystem::exists(extensions_folder)
        && (std::filesystem::is_directory(extensions_folder)
            || std::filesystem::is_symlink(extensions_folder))) {
        // TODO: Check that it points to a directory if its a symlink
        beammp_lua_debugf("Found extensions/: {}", extensions_folder);
        // load extensions from the folder, can't fail
        auto n = load_extensions(extensions_folder);
        beammp_lua_debugf("Loaded {} extensions.", n);
        beammp_lua_debugf("Set up file watcher to watch extensions folder for changes");
        // set up file watcher. this will watch for new extensions or for extensions which have
        // changed (via modification time).
        m_extensions_watcher.watch_files_in(extensions_folder);
        // set up callback for when an extension changes.
        // we simply reload the extension as if nothing happened :)
        // TODO
        /*
        m_extensions_watch_conn = m_extensions_watcher.sig_file_changed.connect_scoped(
            [this, extensions_folder](const std::filesystem::path& path) {
                if (path.extension() != ".lua") {
                    return; // ignore
                }
                auto rel = std::filesystem::relative(path, extensions_folder).string();
                rel = boost::algorithm::replace_all_copy(rel, "/", "_");
                rel = boost::algorithm::replace_all_copy(rel, ".lua", "");
                if (!check_name_validity(rel)) {
                    beammp_lua_errorf("Can't load/reload extension at path: {}. The resulting extension name would be invalid.", path);
                } else {
                    load_extension(path, rel);
                }
            });
        */
    } else {
        beammp_lua_debugf("Plugin '{}' has no extensions.", name());
    }
    auto main_lua = m_path / "main.lua";
    if (std::filesystem::exists(main_lua)) {
        // TODO: Check that it's a regular file or symlink
        beammp_lua_debugf("Found main.lua: {}", main_lua.string());
        boost::asio::post(m_io, [this, main_lua] {
            try {
                m_state.safe_script_file(main_lua.string());
            } catch (const std::exception& e) {
                beammp_lua_errorf("Error running '{}': {}", main_lua.string(), e.what());
            }
        });
    } else {
        beammp_lua_warnf("No 'main.lua' found, a plugin should have a 'main.lua'.");
    }
    return {};
}

Error LuaPlugin::initialize_overrides() {
    boost::asio::post(m_io, [this] {
        m_state.globals()["print"] = [this](sol::variadic_args args) {
            l_print(args);
        };
    });

    Error err = fix_lua_paths();
    if (err) {
        return err;
    }

    return {};
}

Error LuaPlugin::fix_lua_paths() {
    std::stringstream lua_paths;
    std::stringstream lua_c_paths;
    std::vector<std::filesystem::path> relevant_paths = {
        m_path,
        m_path / "extensions",
    };
    for (const auto& Path : relevant_paths) {
        lua_paths << ";" << (Path / "?.lua").string();
        lua_paths << ";" << (Path / "lua/?.lua").string();
#if WIN32
        lua_c_paths << ";" << (Path / "?.dll").string();
        lua_c_paths << ";" << (Path / "lib/?.dll").string();
#else // unix
        lua_c_paths << ";" << (Path / "?.so").string();
        lua_c_paths << ";" << (Path / "lib/?.so").string();
#endif
    }
    auto package_table = m_state.globals().get<sol::table>("package");
    package_table["path"] = package_table.get<std::string>("path") + lua_paths.str();
    package_table["cpath"] = package_table.get<std::string>("cpath") + lua_c_paths.str();
    m_state.globals()["package"] = package_table;

    return {};
}

void LuaPlugin::load_extension(const std::filesystem::path& file, const std::string& ext_name) {
    // we have to assume that load_extension may be called at any time, even to reload an existing extension.
    // thus, it cannot make assumptions about the plugin's status or state.
    beammp_lua_debugf("Loading extension '{}' from {}", ext_name, file);
    // save the extension in a list to make it queryable
    m_known_extensions->insert_or_assign(ext_name, file);
    // extension names, generated by the caller, must be valid lua identifiers
    if (!check_name_validity(ext_name)) {
        beammp_lua_errorf("Extension name '{}' is invalid. Please make sure the extension and it's folder(s) do not contain special characters, spaces, start with a digit, or similar.", ext_name);
        return;
    }
    try {
        auto result = m_state.safe_script_file(file.string());
        if (!result.valid()) {
            beammp_lua_errorf("Error loading extension '{}' from {}. Running file resulted in invalid state: {}. Please check for errors in the lines before this message", ext_name, file, sol::to_string(result.status()));
            return;
        } else if (result.get_type() != sol::type::table) {
            beammp_lua_errorf("Error loading extension '{}' from {}: Expected extension to return a table, got {} instead.", ext_name, file, sol::type_name(m_state.lua_state(), result.get_type()));
            return;
        }
        auto M = result.get<sol::table>();
        m_state.globals()[ext_name] = M;
    } catch (const std::exception& e) {
        beammp_lua_errorf("Error loading extension '{}' from {}: {}", ext_name, file, e.what());
        return;
    }
    beammp_lua_debugf("Extension '{}' loaded!", ext_name);
}

size_t LuaPlugin::load_extensions(const std::filesystem::path& extensions_folder, const std::string& base) {
    std::filesystem::directory_iterator iter(extensions_folder, s_directory_iteration_options);
    std::vector<std::filesystem::directory_entry> files;
    std::vector<std::filesystem::directory_entry> directories;
    for (const auto& entry : iter) {
        if (entry.is_directory()) {
            directories.push_back(entry);
        } else if (entry.is_regular_file()) {
            files.push_back(entry);
        } else {
            beammp_lua_tracef("{} is neither a file nor a directory, skipping", entry.path());
        }
    }
    // sort files alphabetically
    std::sort(files.begin(), files.end(), [&](const auto& a, const auto& b) {
        auto as = a.path().filename().string();
        auto bs = b.path().filename().string();
        return std::lexicographical_compare(as.begin(), as.end(), bs.begin(), bs.end());
    });
    for (const auto& file : files) {
        boost::asio::post(m_io, [this, base, file] {
            std::string ext_name;
            if (base.empty()) {
                ext_name = file.path().stem().string();
            } else {
                ext_name = fmt::format("{}_{}", base, file.path().stem().string());
            }
            load_extension(file, ext_name);
        });
    }
    std::sort(directories.begin(), directories.end(), [&](const auto& a, const auto& b) {
        auto as = a.path().filename().string();
        auto bs = b.path().filename().string();
        return std::lexicographical_compare(as.begin(), as.end(), bs.begin(), bs.end());
    });
    size_t count = 0;
    for (const auto& dir : directories) {
        std::string ext_name = dir.path().filename().string();
        std::filesystem::path path = dir.path();
        count += load_extensions(path, ext_name);
    }
    return count + files.size();
}

void LuaPlugin::thread_main() {
    RegisterThread(name());
    beammp_lua_debugf("Waiting for initialization");
    // wait for interruption
    // we sleep for some time, which can be interrupted by a thread.interrupt(),
    // which will cause the sleep to throw a boost::thread_interrupted exception.
    // we (ab)use this to synchronize.
    try {
        boost::this_thread::sleep_for(boost::chrono::seconds(1));
    } catch (boost::thread_interrupted) {
    }
    beammp_lua_debugf("Initialized!");
    while (!*m_shutdown) {
        auto ran = m_io.run_for(std::chrono::seconds(5));
        // update the memory used by the Lua Plugin, then immediately resume execution of handlers
        if (ran != 0) {
            *m_memory = m_state.memory_used();
        }
    }
}

Error LuaPlugin::initialize() {
    Error err = initialize_error_handlers();
    if (err) {
        return { "Failed to initialize error handlers: {}", err };
    }
    err = initialize_libraries();
    if (err) {
        return { "Failed to initialize libraries: {}", err };
    }
    err = initialize_overrides();
    if (err) {
        return { "Failed to initialize overrides: {}", err };
    }
    err = load_files();
    if (err) {
        return { "Failed to load initial files: {}", err };
    }

    // interrupt the thread, signalling it to start
    m_thread.interrupt();
    return {};
}

Error LuaPlugin::cleanup() {
    // TODO
    return {};
}

Error LuaPlugin::reload() {
    // TODO
    return {};
}

std::string LuaPlugin::name() const {
    return m_path.stem().string();
}

std::filesystem::path LuaPlugin::path() const {
    return m_path;
}

std::shared_future<std::vector<Value>> LuaPlugin::handle_event(const std::string& event_name, const std::shared_ptr<Value>& args) {
    std::shared_ptr<std::promise<std::vector<Value>>> promise = std::make_shared<std::promise<std::vector<Value>>>();
    std::shared_future<std::vector<Value>> futures { promise->get_future() };
    boost::asio::post(m_io, [this, event_name, args, promise, futures] {
        std::vector<Value> results {};
        if (m_event_handlers_named.contains(event_name)) {
            auto handler_names = m_event_handlers_named.at(event_name);
            for (const auto& handler_name : handler_names) {
                try {
                    if (m_state.globals()[handler_name].valid() && m_state.globals()[handler_name].get_type() == sol::type::function) {
                        auto fn = m_state.globals().get<sol::protected_function>(handler_name);
                        fn.set_error_handler(m_state.globals()[ERR_HANDLER]);
                        auto lua_args = boost::apply_visitor(ValueToLuaVisitor(m_state), *args);
                        sol::protected_function_result res;

                        if (args->which() == VALUE_TYPE_IDX_TUPLE) {
                            res = fn(sol::as_args(lua_args.as<std::vector<sol::object>>()));
                        } else {
                            res = fn(lua_args);
                        }
                        if (res.valid()) {
                            auto maybe_res = sol_obj_to_value(res.get<sol::object>());
                            if (maybe_res) {
                                results.emplace_back(maybe_res.move());
                            } else {
                                beammp_lua_errorf("Error using return value from event handler '{}' for event '{}': {}", handler_name, event_name, maybe_res.error);
                            }
                        }
                    } else {
                        beammp_lua_errorf("Invalid event handler '{}' for event '{}': Handler either doesn't exist or isn't a global function", handler_name, event_name);
                    }
                } catch (const std::exception& e) {
                    beammp_lua_errorf("Error finding and running event handler for event '{}': {}. It was called with argument(s): {}", event_name, e.what(), boost::apply_visitor(ValueToStringVisitor(), *args));
                }
            }
        }
        if (m_event_handlers.contains(event_name)) {
            auto handlers = m_event_handlers.at(event_name);
            for (const auto& fn : handlers) {
                try {
                    auto lua_args = boost::apply_visitor(ValueToLuaVisitor(m_state), *args);
                    sol::protected_function_result res;

                    if (args->which() == VALUE_TYPE_IDX_TUPLE) {
                        res = fn(sol::as_args(lua_args.as<std::vector<sol::object>>()));
                    } else {
                        res = fn(lua_args);
                    }
                    if (res.valid()) {
                        auto maybe_res = sol_obj_to_value(res.get<sol::object>());
                        if (maybe_res) {
                            results.emplace_back(maybe_res.move());
                        } else {
                            beammp_lua_errorf("Error using return value from event handler '<<lua function {:p}>>' for event '{}': {}", fn.pointer(), event_name, maybe_res.error);
                        }
                    }
                } catch (const std::exception& e) {
                    beammp_lua_errorf("Error finding and running event handler for event '{}': {}. It was called with argument(s): {}", event_name, e.what(), boost::apply_visitor(ValueToStringVisitor(), *args));
                }
            }
        }
        promise->set_value(std::move(results));
    });
    return futures;
}

size_t LuaPlugin::memory_usage() const {
    return *m_memory;
}

std::shared_ptr<Timer> LuaPlugin::make_timer(size_t ms) {
    m_timers.push_back(std::make_shared<Timer>(boost::asio::deadline_timer(m_io), ms));
    auto timer = m_timers.back();
    timer->timer.expires_from_now(timer->interval);
    std::sort(m_timers.begin(), m_timers.end());
    return timer;
}

void LuaPlugin::cancel_timer(const std::shared_ptr<Timer>& timer) {
    auto iter = std::find(m_timers.begin(), m_timers.end(), timer);
    if (iter != m_timers.end()) {
        m_timers.erase(iter);
        timer->timer.cancel();
    } else {
        timer->timer.cancel();
        beammp_lua_debugf("Failed to remove timer (already removed)");
    }
}

void LuaPlugin::l_print(const sol::variadic_args& args) {
    auto result = print_impl(args);
    beammp_lua_infof("{}", result);
}

void LuaPlugin::l_mp_schedule_call_helper(const boost::system::error_code& err, std::shared_ptr<Timer> timer, const sol::function& fn, std::shared_ptr<ValueTuple> args) {
    if (err) {
        beammp_lua_debugf("MP.schedule_call_repeat: {}", err.what());
        return;
    }
    timer->timer.expires_from_now(timer->interval);
    sol::protected_function prot(fn);
    prot.set_error_handler(m_state.globals()[ERR_HANDLER]);
    std::vector<sol::object> objs;
    objs.reserve(args->size());
    for (const auto& val : *args) {
        objs.push_back(boost::apply_visitor(ValueToLuaVisitor(m_state), val));
    }
    prot(sol::as_args(objs));
    timer->timer.async_wait([this, timer, fn, args](const auto& err) {
        l_mp_schedule_call_helper(err, timer, fn, args);
    });
}

void LuaPlugin::l_mp_schedule_call_once(size_t ms, const sol::function& fn, sol::variadic_args args) {
    auto timer = make_timer(ms);
    std::vector<sol::object> args_vec(args.begin(), args.end());
    std::shared_ptr<ValueTuple> tuple = std::make_shared<ValueTuple>();
    tuple->reserve(args_vec.size());
    for (const auto& obj : args_vec) {
        auto res = sol_obj_to_value(obj);
        if (res) [[likely]] {
            tuple->emplace_back(res.move());
        } else {
            beammp_lua_errorf("Can't serialize an argument across boundaries (for passing to a MP.schedule_call_* later): ", res.error);
            tuple->emplace_back(Null {});
        }
    }
    timer->timer.async_wait([this, timer, fn, tuple](const auto& err) {
        if (err) {
            beammp_lua_debugf("MP.schedule_call_once: {}", err.what());
            return;
        }
        sol::protected_function prot(fn);
        prot.set_error_handler(m_state.globals()[ERR_HANDLER]);
        std::vector<sol::object> objs;
        objs.reserve(tuple->size());
        for (const auto& val : *tuple) {
            objs.push_back(boost::apply_visitor(ValueToLuaVisitor(m_state), val));
        }
        beammp_lua_debugf("Calling with {} args", objs.size());
        prot(sol::as_args(objs));
        cancel_timer(timer);
    });
}

std::shared_ptr<Timer> LuaPlugin::l_mp_schedule_call_repeat(size_t ms, const sol::function& fn, sol::variadic_args args) {
    auto timer = make_timer(ms);
    // TODO: Cleanly transfer invalid objects
    std::vector<sol::object> args_vec(args.begin(), args.end());
    std::shared_ptr<ValueTuple> tuple = std::make_shared<ValueTuple>();
    tuple->reserve(args_vec.size());
    for (const auto& obj : args_vec) {
        auto res = sol_obj_to_value(obj);
        if (res) [[likely]] {
            tuple->emplace_back(res.move());
        } else {
            beammp_lua_errorf("Can't serialize an argument across boundaries (for passing to a MP.schedule_call_* later): ", res.error);
            tuple->emplace_back(Null {});
        }
    }
    timer->timer.async_wait([this, timer, fn, tuple](const auto& err) {
        l_mp_schedule_call_helper(err, timer, fn, tuple);
    });
    return timer;
}

std::string LuaPlugin::print_impl(const sol::variadic_args& args) {
    auto obj_args = std::vector<sol::object>(args.begin(), args.end());
    std::string result {};
    result.reserve(500);

    // used as the invalid value provider in sol_obj_to_value.
    auto special_stringifier = [](const sol::object& object) -> Result<Value> {
        switch (object.get_type()) {
        case sol::type::none:
        case sol::type::lua_nil:
        case sol::type::string:
        case sol::type::number:
        case sol::type::boolean:
        case sol::type::table:
            // covered by value to string visitor
            break;
        case sol::type::thread:
            return { fmt::format("<<lua thread: {:p}>>", object.pointer()) };
        case sol::type::function:
            return { fmt::format("<<lua function: {:p}>>", object.pointer()) };
        case sol::type::userdata:
            return { fmt::format("<<lua userdata: {:p}>>", object.pointer()) };
        case sol::type::lightuserdata:
            return { fmt::format("<<lua lightuserdata: {:p}>>", object.pointer()) };
        case sol::type::poly:
            return { fmt::format("<<lua poly: {:p}>>", object.pointer()) };
        default:
            break;
        }
        return { fmt::format("<<lua unknown type: {:p}>>", object.pointer()) };
    };

    for (const auto& obj : obj_args) {
        auto maybe_val = sol_obj_to_value(obj, special_stringifier);
        if (maybe_val) {
            result += boost::apply_visitor(ValueToStringVisitor(ValueToStringVisitor::Flag::NONE), maybe_val.move());
            result += " ";
        } else {
            beammp_lua_errorf("Failed to print() an argument: {}", maybe_val.error);
        }
    }
    return result;
}
