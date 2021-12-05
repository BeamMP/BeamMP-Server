#include "TConsole.h"
#include "Common.h"
#include "Compat.h"

#include "Client.h"
#include "CustomAssert.h"
#include "LuaAPI.h"
#include "TLuaEngine.h"

#include <ctime>
#include <sstream>

static inline bool StringStartsWith(const std::string& What, const std::string& StartsWith) {
    return What.size() >= StartsWith.size() && What.substr(0, StartsWith.size()) == StartsWith;
}

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

void TConsole::ChangeToLuaConsole(const std::string& LuaStateId) {
    if (!mIsLuaConsole) {
        mStateId = LuaStateId;
        mIsLuaConsole = true;
        if (mStateId != mDefaultStateId) {
            Application::Console().WriteRaw("Entered Lua console for state '" + mStateId + "'. To exit, type `exit()`");
        } else {
            Application::Console().WriteRaw("Entered Lua console. To exit, type `exit()`");
        }
        mCachedRegularHistory = mCommandline.history();
        mCommandline.set_history(mCachedLuaHistory);
        mCommandline.set_prompt("lua> ");
    }
}

void TConsole::ChangeToRegularConsole() {
    if (mIsLuaConsole) {
        mIsLuaConsole = false;
        if (mStateId != mDefaultStateId) {
            Application::Console().WriteRaw("Left Lua console for state '" + mStateId + "'.");
        } else {
            Application::Console().WriteRaw("Left Lua console.");
        }
        mCachedLuaHistory = mCommandline.history();
        mCommandline.set_history(mCachedRegularHistory);
        mCommandline.set_prompt("> ");
        mStateId = mDefaultStateId;
    }
}

void TConsole::Command_Lua(const std::string& cmd) {
    if (cmd.size() > 3) {
        auto NewStateId = cmd.substr(4);
        beammp_assert(!NewStateId.empty());
        if (mLuaEngine->HasState(NewStateId)) {
            ChangeToLuaConsole(NewStateId);
        } else {
            Application::Console().WriteRaw("Lua state '" + NewStateId + "' is not a known state. Didn't switch to Lua.");
        }
    } else {
        ChangeToLuaConsole(mDefaultStateId);
    }
}

void TConsole::Command_Help(const std::string&) {
    static constexpr const char* sHelpString = R"(
    Commands:
        help                    displays this help
        exit                    shuts down the server
        kick <name> [reason]    kicks specified player with an optional reason
        list                    lists all players and info about them
        say <message>           sends the message to all players in chat
        lua [state id]          switches to lua, optionally into a specific state id's lua
        status                  how the server is doing and what it's up to)";
    Application::Console().WriteRaw("BeamMP-Server Console: " + std::string(sHelpString));
}

void TConsole::Command_Kick(const std::string& cmd) {
    if (cmd.size() > 4) {
        auto Name = cmd.substr(5);
        std::string Reason = "Kicked by server console";
        auto SpacePos = Name.find(' ');
        if (SpacePos != Name.npos) {
            Reason = Name.substr(SpacePos + 1);
            Name = cmd.substr(5, cmd.size() - Reason.size() - 5 - 1);
        }
        beammp_trace("attempt to kick '" + Name + "' for '" + Reason + "'");
        bool Kicked = false;
        auto NameCompare = [](std::string Name1, std::string Name2) -> bool {
            std::for_each(Name1.begin(), Name1.end(), [](char& c) { c = tolower(c); });
            std::for_each(Name2.begin(), Name2.end(), [](char& c) { c = tolower(c); });
            return StringStartsWith(Name1, Name2) || StringStartsWith(Name2, Name1);
        };
        mLuaEngine->Server().ForEachClient([&](std::weak_ptr<TClient> Client) -> bool {
            if (!Client.expired()) {
                auto locked = Client.lock();
                if (NameCompare(locked->GetName(), Name)) {
                    mLuaEngine->Network().ClientKick(*locked, Reason);
                    Kicked = true;
                    return false;
                }
            }
            return true;
        });
        if (!Kicked) {
            Application::Console().WriteRaw("Error: No player with name matching '" + Name + "' was found.");
        } else {
            Application::Console().WriteRaw("Kicked player '" + Name + "' for reason: '" + Reason + "'.");
        }
    }
}

void TConsole::Command_Say(const std::string& cmd) {
    if (cmd.size() > 3) {
        auto Message = cmd.substr(4);
        LuaAPI::MP::SendChatMessage(-1, Message);
    }
}

void TConsole::Command_List(const std::string&) {
    if (mLuaEngine->Server().ClientCount() == 0) {
        Application::Console().WriteRaw("No players online.");
    } else {
        std::stringstream ss;
        ss << std::left << std::setw(25) << "Name" << std::setw(6) << "ID" << std::setw(6) << "Cars" << std::endl;
        mLuaEngine->Server().ForEachClient([&](std::weak_ptr<TClient> Client) -> bool {
            if (!Client.expired()) {
                auto locked = Client.lock();
                ss << std::left << std::setw(25) << locked->GetName()
                   << std::setw(6) << locked->GetID()
                   << std::setw(6) << locked->GetCarCount() << "\n";
            }
            return true;
        });
        auto Str = ss.str();
        Application::Console().WriteRaw(Str.substr(0, Str.size() - 1));
    }
}

void TConsole::Command_Status(const std::string&) {
    std::stringstream Status;

    size_t CarCount = 0;
    size_t ConnectedCount = 0;
    size_t GuestCount = 0;
    size_t SyncedCount = 0;
    size_t SyncingCount = 0;
    size_t MissedPacketQueueSum = 0;
    int LargestSecondsSinceLastPing = 0;
    mLuaEngine->Server().ForEachClient([&](std::weak_ptr<TClient> Client) -> bool {
        if (!Client.expired()) {
            auto Locked = Client.lock();
            CarCount += Locked->GetCarCount();
            ConnectedCount += Locked->IsConnected() ? 1 : 0;
            GuestCount += Locked->IsGuest() ? 1 : 0;
            SyncedCount += Locked->IsSynced() ? 1 : 0;
            SyncingCount += Locked->IsSyncing() ? 1 : 0;
            MissedPacketQueueSum += Locked->MissedPacketQueueSize();
            if (Locked->SecondsSinceLastPing() < LargestSecondsSinceLastPing) {
                LargestSecondsSinceLastPing = Locked->SecondsSinceLastPing();
            }
        }
        return true;
    });

    auto ElapsedTime = mLuaEngine->Server().UptimeTimer.GetElapsedTime();

    Status << "BeamMP-Server Status:\n"
           << "\tTotal Players:             " << mLuaEngine->Server().ClientCount() << "\n"
           << "\tSyncing Players:           " << SyncingCount << "\n"
           << "\tSynced Players:            " << SyncedCount << "\n"
           << "\tConnected Players:         " << ConnectedCount << "\n"
           << "\tGuests:                    " << GuestCount << "\n"
           << "\tCars:                      " << CarCount << "\n"
           << "\tUptime:                    " << ElapsedTime << "ms (~" << size_t(ElapsedTime / 1000.0 / 60.0 / 60.0) << "h) \n"
           << "\tLua:\n"
           << "\t\tQueued results to check:     " << mLuaEngine->GetResultsToCheckSize() << "\n"
           << "\t\tStates:                      " << mLuaEngine->GetLuaStateCount() << "\n"
           << "\t\tEvent timers:                " << mLuaEngine->GetTimedEventsCount() << "\n"
           << "\t\tEvent handlers:              " << mLuaEngine->GetRegisteredEventHandlerCount() << "\n"
           << "";

    Application::Console().WriteRaw(Status.str());
}

void TConsole::RunAsCommand(const std::string& cmd, bool IgnoreNotACommand) {
    auto FutureIsNonNil =
        [](const std::shared_ptr<TLuaResult>& Future) {
            if (!Future->Error && Future->Result.valid()) {
                auto Type = Future->Result.get_type();
                return Type != sol::type::lua_nil && Type != sol::type::none;
            }
            return false;
        };
    std::vector<std::shared_ptr<TLuaResult>> NonNilFutures;
    { // Futures scope
        auto Futures = mLuaEngine->TriggerEvent("onConsoleInput", "", cmd);
        TLuaEngine::WaitForAll(Futures, std::chrono::seconds(5));
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
        if (!IgnoreNotACommand) {
            Application::Console().WriteRaw("Error: Unknown command: '" + cmd + "'. Type 'help' to see a list of valid commands.");
        }
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
                    } else if (StringStartsWith(cmd, "lua")) {
                        Command_Lua(cmd);
                    } else if (StringStartsWith(cmd, "help")) {
                        RunAsCommand(cmd, true);
                        Command_Help(cmd);
                    } else if (StringStartsWith(cmd, "kick")) {
                        RunAsCommand(cmd, true);
                        Command_Kick(cmd);
                    } else if (StringStartsWith(cmd, "say")) {
                        RunAsCommand(cmd, true);
                        Command_Say(cmd);
                    } else if (StringStartsWith(cmd, "list")) {
                        RunAsCommand(cmd, true);
                        Command_List(cmd);
                    } else if (StringStartsWith(cmd, "status")) {
                        RunAsCommand(cmd, true);
                        Command_Status(cmd);
                    } else if (!cmd.empty()) {
                        RunAsCommand(cmd);
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
    Engine.EnsureStateExists(mDefaultStateId, "Console");
}
