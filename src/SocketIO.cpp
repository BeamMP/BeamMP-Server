#include "SocketIO.h"
#include "Logger.h"

#include <signal.h>

SocketIO::SocketIO()
    : _Thread(std::bind(&SocketIO::ThreadMain, this)) {
    _Client.connect("url goes here");
    _Client.set_logs_quiet();
}

SocketIO::~SocketIO() {
    _CloseThread.store(true);
    _Thread.join();
}

void SocketIO::Emit(const std::string& EventName, const std::string& Data) {
    debug("emitting event " + EventName + " with data: \"" + Data + "\"");
    std::unique_lock Lock(_QueueMutex);
    _Queue.push_back({ EventName, Data });
    debug("queue now has " + std::to_string(_Queue.size()) + " events");
}

void SocketIO::ThreadMain() {
    DebugPrintTID();
    while (!_CloseThread.load()) {
        bool empty = false;
        { // queue lock scope
            std::unique_lock Lock(_QueueMutex);
            empty = _Queue.empty();
        } // end queue lock scope
        if (empty) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        } else {
            std::pair<std::string, std::string> NameDataPair;
            { // queue lock scope
                std::unique_lock Lock(_QueueMutex);
                NameDataPair = _Queue.front();
                _Queue.pop_front();
            } // end queue lock scope
            debug("sending " + NameDataPair.first);
            _Client.socket()->emit(NameDataPair.first, NameDataPair.second);
            debug("sent " + NameDataPair.first);
        }
    }
    std::cout << "closing " + std::string(__func__) << std::endl;
    _Client.close();
}
