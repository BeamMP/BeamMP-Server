#include "TConsole.h"
#include "Common.h"
#include "Compat.h"

#include "TLuaEngine.h"

#include <ctime>
#include <sstream>
#include <zip.h>

std::string GetDate() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    auto local_tm = std::localtime(&tt);
    char buf[30];
    std::string date;
#if defined(DEBUG)
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
#endif
        std::strftime(buf, sizeof(buf), "[%d/%m/%y %T] ", local_tm);
        date += buf;
#if defined(DEBUG)
    }
#endif

    return date;
}

void TConsole::BackupOldLog() {
    fs::path Path = "Server.log";
    if (fs::exists(Path)) {
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
        auto cmd = c.get_command();
        mCommandline.write("> " + cmd);
        if (cmd == "exit") {
            beammp_info("gracefully shutting down");
            Application::GracefullyShutdown();
        } else if (cmd == "clear" || cmd == "cls") {
            // TODO: clear screen
        } else {
            if (!mLuaEngine) {
                beammp_info("Lua not started yet, please try again in a second");
            } else {
                auto Future = mLuaEngine->EnqueueScript(mStateId, { std::make_shared<std::string>(cmd), "", "" });
                while (!Future->Ready) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (Future->Error) {
                    beammp_lua_error(Future->ErrorMessage);
                }
            }
        }
    };
}

void TConsole::Write(const std::string& str) {
    auto ToWrite = GetDate() + str;
    mCommandline.write(ToWrite);
    // TODO write to logfile, too
}

void TConsole::WriteRaw(const std::string& str) {
    mCommandline.write(str);
}

void TConsole::InitializeLuaConsole(TLuaEngine& Engine) {
    mLuaEngine = &Engine;
    Engine.EnsureStateExists(mStateId, "Console");
}
