#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <sio_client.h>
#include <thread>

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

    void Emit(SocketIORoom Room, SocketIOEvent Event, const std::string& Data);

    ~SocketIO();

    void SetAuthenticated(bool auth) { _Authenticated = auth; }

private:
    SocketIO();

    void ThreadMain();

    struct Event {
        std::string Room;
        std::string Name;
        std::string Data;
    };

    bool _Authenticated { false };
    sio::client _Client;
    std::thread _Thread;
    std::atomic_bool _CloseThread { false };
    std::mutex _QueueMutex;
    std::deque<Event> _Queue;

    friend std::unique_ptr<SocketIO> std::make_unique<SocketIO>();
};

