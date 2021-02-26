#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <sio_client.h>
#include <thread>
#include <memory>

/*
 * We send relevant server events over socket.io to the backend.
 *
 * We send all events to `backend.beammp.com`, to the room `/key` 
 * where `key` is the currently active auth-key. 
 */

enum class SocketIOEvent {
    ConsoleOut,
    CPUUsage,
    MemoryUsage,
    NetworkUsage,
    PlayerList,
};

enum class SocketIORoom {
    None,
    Stats,
    Player,
    Info,
    Console,
};

class SocketIO final {
private:
    struct Event;

public:
    enum class EventType {
    };

    // Singleton pattern
    static SocketIO& Get();

    void Emit(SocketIOEvent Event, const std::string& Data);

    ~SocketIO();

    void SetAuthenticated(bool auth) { mAuthenticated = auth; }

private:
    SocketIO() noexcept;

    void ThreadMain();

    struct Event {
        std::string Name;
        std::string Data;
    };

    bool mAuthenticated { false };
    sio::client mClient;
    std::thread mThread;
    std::atomic_bool mCloseThread { false };
    std::mutex mQueueMutex;
    std::deque<Event> mQueue;

    friend std::unique_ptr<SocketIO> std::make_unique<SocketIO>();
};

