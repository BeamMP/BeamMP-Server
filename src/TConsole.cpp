#include "TConsole.h"
#include "Common.h"
#include "Compat.h"

#include "LuaAPI.h"
#include "TLuaEngine.h"

#include <ctime>
#include <sstream>

static inline std::string TrimString(std::string S) {
    S.erase(S.begin(), std::find_if(S.begin(), S.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    S.erase(std::find_if(S.rbegin(), S.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(),
        S.end());
    return S;
}

std::string GetDate() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    auto local_tm = std::localtime(&tt);
    char buf[30];
    std::string date;
    if (Application::Settings.DebugModeEnabled) {
        std::strftime(buf, sizeof(buf), "[%d/%m/%y %T.", local_tm);
        date += buf;
        auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
        auto fraction = now - seconds;
        size_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(fraction).count();
        char fracstr[5];
        std::sprintf(fracstr, "%0.3lu", ms);
        date += fracstr;
        date += "] ";
    } else {
        std::strftime(buf, sizeof(buf), "[%d/%m/%y %T] ", local_tm);
        date += buf;
    }

    return date;
}

void TConsole::BackupOldLog() {
    fs::path Path = "Server.log";
    if (fs::exists(Path)) {
        auto OldLog = Path.filename().stem().string() + ".old.log";
        try {
            fs::rename(Path, OldLog);
            beammp_debug("renamed old log file to '" + OldLog + "'");
        } catch (const std::exception& e) {
            beammp_warn(e.what());
        }
        /*
        int err = 0;
        zip* z = zip_open("ServerLogs.zip", ZIP_CREATE, &err);
        if (!z) {
            std::cerr << GetPlatformAgnosticErrorString() << std::endl;
            return;
        }
        FILE* File = std::fopen(Path.string().c_str(), "r");
        if (!File) {
            std::cerr << GetPlatformAgnosticErrorString() << std::endl;
            return;
        }
        std::vector<uint8_t> Buffer;
        Buffer.resize(fs::file_size(Path));
        std::fread(Buffer.data(), 1, Buffer.size(), File);
        std::fclose(File);

        auto s = zip_source_buffer(z, Buffer.data(), Buffer.size(), 0);

        auto TimePoint = fs::last_write_time(Path);
        auto Secs = TimePoint.time_since_epoch().count();
        auto MyTimeT = std::time(&Secs);

        std::string NewName = Path.stem().string();
        NewName += "_";
        std::string Time;
        Time.resize(32);
        size_t n = strftime(Time.data(), Time.size(), "%F_%H.%M.%S", localtime(&MyTimeT));
        Time.resize(n);
        NewName += Time;
        NewName += ".log";

        zip_file_add(z, NewName.c_str(), s, 0);
        zip_close(z);
    */
    }
}

void TConsole::ChangeToLuaConsole() {
    if (!mIsLuaConsole) {
        mIsLuaConsole = true;
        Application::Console().WriteRaw("Entered Lua console. To exit, type `exit()`");
        mCachedRegularHistory = mCommandline.history();
        mCommandline.set_history(mCachedLuaHistory);
        mCommandline.set_prompt("lua> ");
    }
}

void TConsole::ChangeToRegularConsole() {
    if (mIsLuaConsole) {
        mIsLuaConsole = false;
        Application::Console().WriteRaw("Left Lua console.");
        mCachedLuaHistory = mCommandline.history();
        mCommandline.set_history(mCachedRegularHistory);
        mCommandline.set_prompt("> ");
    }
}

TConsole::TConsole() {
    mCommandline.enable_history();
    mCommandline.set_history_limit(20);
    mCommandline.set_prompt("> ");
    BackupOldLog();
    bool success = mCommandline.enable_write_to_file("Server.log");
    if (!success) {
        beammp_error("unable to open file for writing: \"Server.log\"");
    }
    mCommandline.on_command = [this](Commandline& c) {
        try {
            auto cmd = c.get_command();
            cmd = TrimString(cmd);
            mCommandline.write(mCommandline.prompt() + cmd);
            if (!mLuaEngine) {
                beammp_info("Lua not started yet, please try again in a second");
            } else {
                if (mIsLuaConsole) {
                    if (cmd == "exit()") {
                        ChangeToRegularConsole();
                    } else {
                        auto Future = mLuaEngine->EnqueueScript(mStateId, { std::make_shared<std::string>(cmd), "", "" });
                        while (!Future->Ready) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // TODO: Add a timeout
                        }
                        if (Future->Error) {
                            beammp_lua_error(Future->ErrorMessage);
                        }
                    }
                } else {
                    if (cmd == "exit") {
                        beammp_info("gracefully shutting down");
                        Application::GracefullyShutdown();
                    } else if (cmd == "lua") {
                        ChangeToLuaConsole();
                    } else if (!cmd.empty()) {
                        auto FutureIsNonNil =
                            [](const std::shared_ptr<TLuaResult>& Future) {
                                if (!Future->Error) {
                                    auto Type = Future->Result.get_type();
                                    return Type != sol::type::lua_nil && Type != sol::type::none;
                                }
                                return false;
                            };
                        std::vector<std::shared_ptr<TLuaResult>> NonNilFutures;
                        { // Futures scope
                            auto Futures = mLuaEngine->TriggerEvent("onConsoleInput", "", cmd);
                            TLuaEngine::WaitForAll(Futures);
                            size_t Count = 0;
                            for (auto& Future : Futures) {
                                if (!Future->Error) {
                                    ++Count;
                                }
                            }
                            for (const auto& Future : Futures) {
                                if (FutureIsNonNil(Future)) {
                                    NonNilFutures.push_back(Future);
                                }
                            }
                        }
                        if (NonNilFutures.size() == 0) {
                            Application::Console().WriteRaw("Error: Unknown command: '" + cmd + "'");
                        } else {
                            std::stringstream Reply;
                            if (NonNilFutures.size() > 1) {
                                for (size_t i = 0; i < NonNilFutures.size(); ++i) {
                                    Reply << NonNilFutures[i]->StateId << ": \n"
                                          << LuaAPI::LuaToString(NonNilFutures[i]->Result);
                                    if (i < NonNilFutures.size() - 1) {
                                        Reply << "\n";
                                    }
                                }
                            } else {
                                Reply << LuaAPI::LuaToString(NonNilFutures[0]->Result);
                            }
                            Application::Console().WriteRaw(Reply.str());
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            beammp_error("Console died with: " + std::string(e.what()) + ". This could be a fatal error and could cause the server to terminate.");
        }
    };
}

void TConsole::Write(const std::string& str) {
    auto ToWrite = GetDate() + str;
    mCommandline.write(ToWrite);
}

void TConsole::WriteRaw(const std::string& str) {
    mCommandline.write(str);
}

void TConsole::InitializeLuaConsole(TLuaEngine& Engine) {
    mLuaEngine = &Engine;
    Engine.EnsureStateExists(mStateId, "Console");
}
