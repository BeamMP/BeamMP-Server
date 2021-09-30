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
    return date.str();
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
