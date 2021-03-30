#include "TConsole.h"
#include "Common.h"
#include "Compat.h"

#include <ctime>
#include <sstream>

std::string GetDate() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    tm local_tm {};
#ifdef WIN32
    localtime_s(&local_tm, &tt);
#else // unix
    localtime_r(&tt, &local_tm);
#endif // WIN32
    std::stringstream date;
    int S = local_tm.tm_sec;
    int M = local_tm.tm_min;
    int H = local_tm.tm_hour;
    std::string Secs = (S > 9 ? std::to_string(S) : "0" + std::to_string(S));
    std::string Min = (M > 9 ? std::to_string(M) : "0" + std::to_string(M));
    std::string Hour = (H > 9 ? std::to_string(H) : "0" + std::to_string(H));
    date
        << "["
        << local_tm.tm_mday << "/"
        << local_tm.tm_mon + 1 << "/"
        << local_tm.tm_year + 1900 << " "
        << Hour << ":"
        << Min << ":"
        << Secs
        << "] ";
    /* TODO
    if (Debug) {
        date << ThreadName()
             << " ";
    }
    */
    warn(date.rdbuf()->str());
    return std::string(date.str());
}

TConsole::TConsole() {
    mCommandline.enable_history();
    mCommandline.set_history_limit(20);
    mCommandline.set_prompt("> ");
    bool success = mCommandline.enable_write_to_file("Server.log");
    if (!success) {
        error("unable to open file for writing: \"Server.log\"");
    }
    mCommandline.on_command = [this](Commandline& c) {
        auto cmd = c.get_command();
        mCommandline.write("> " + cmd);
        if (cmd == "exit") {
            info("gracefully shutting down");
            Application::GracefullyShutdown();
        } else if (cmd == "clear" || cmd == "cls") {
            // TODO: clear screen
        } else {
            if (mLuaConsole) {
                mLuaConsole->Execute(cmd);
            } else {
                error("Lua subsystem not yet initialized, please wait a few seconds and try again");
            }
        }
    };
}

void TConsole::Write(const std::string& str) {
    auto ToWrite = GetDate() + str;
    mCommandline.write(ToWrite);
    // TODO write to logfile, too
}
void TConsole::InitializeLuaConsole(TLuaEngine& Engine) {
    mLuaConsole = std::make_unique<TLuaFile>(Engine, true);
}
void TConsole::WriteRaw(const std::string& str) {
    mCommandline.write(str);
}
