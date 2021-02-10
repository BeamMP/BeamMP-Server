#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <sio_client.h>
#include <thread>

class SocketIO final {
public:
    // Singleton pattern
    static SocketIO& Get() {
        static SocketIO SocketIOInstance;
        return SocketIOInstance;
    }

    void Emit(const std::string& EventName, const std::string& Data);

private:
    SocketIO();
    ~SocketIO();

    void ThreadMain();

    sio::client _Client;
    std::thread _Thread;
    std::atomic_bool _CloseThread { false };
    std::mutex _QueueMutex;
    std::deque<std::pair<std::string, std::string>> _Queue;
};

