#include "SocketIO.h"
#include "Logger.h"
#include "Settings.h"

static std::unique_ptr<SocketIO> SocketIOInstance = std::make_unique<SocketIO>();

SocketIO& SocketIO::Get() {
    return *SocketIOInstance;
}

SocketIO::SocketIO()
    : _Thread([this] { ThreadMain(); }) {
    _Client.socket("/")->on("Hello", [&](sio::event&) {
        DebugPrintTIDInternal("Hello-handler");
        info("Got 'Hello' from backend socket-io!");
    });
    _Client.connect("https://backend.beammp.com");
    _Client.set_logs_quiet();
}

SocketIO::~SocketIO() {
    _CloseThread.store(true);
    _Thread.join();
}

static constexpr auto RoomNameFromEnum(SocketIORoom Room) {
    switch (Room) {
    case SocketIORoom::None:
        return "";
    case SocketIORoom::Console:
        return "console";
    case SocketIORoom::Info:
        return "info";
    case SocketIORoom::Player:
        return "player";
    case SocketIORoom::Stats:
        return "stats";
    default:
        error("unreachable code reached (developer error)");
        abort();
    }
}

static constexpr auto EventNameFromEnum(SocketIOEvent Event) {
    switch (Event) {
    case SocketIOEvent::CPUUsage:
        return "cpu usage";
    case SocketIOEvent::MemoryUsage:
        return "memory usage";
    case SocketIOEvent::ConsoleOut:
        return "console out";
    case SocketIOEvent::NetworkUsage:
        return "network usage";
    case SocketIOEvent::PlayerList:
        return "player list";
    default:
        error("unreachable code reached (developer error)");
        abort();
    }
}

void SocketIO::Emit(SocketIORoom Room, SocketIOEvent Event, const std::string& Data) {
    if (!_Authenticated) {
        debug("trying to emit a socket.io event when not yet authenticated");
        return;
    }
    std::string RoomName = RoomNameFromEnum(Room);
    std::string EventName = EventNameFromEnum(Event);
    debug("emitting event \"" + EventName + "\" with data: \"" + Data + "\" in room \"/key/" + RoomName + "\"");
    std::unique_lock Lock(_QueueMutex);
    _Queue.push_back({ RoomName, EventName, Data });
    debug("queue now has " + std::to_string(_Queue.size()) + " events");
}

void SocketIO::ThreadMain() {
    bool FirstTime = true;
    while (!_CloseThread.load()) {
        if (_Authenticated && FirstTime) {
            FirstTime = false;
            DebugPrintTID();
        }
        bool empty = false;
        { // queue lock scope
            std::unique_lock Lock(_QueueMutex);
            empty = _Queue.empty();
        } // end queue lock scope
        if (empty) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        } else {
            Event TheEvent;
            { // queue lock scope
                std::unique_lock Lock(_QueueMutex);
                TheEvent = _Queue.front();
                _Queue.pop_front();
            } // end queue lock scope
            debug("sending \"" + TheEvent.Name + "\" event");
            auto Room = "/" + TheEvent.Room;
            _Client.socket("/")->emit(TheEvent.Name, TheEvent.Data);
            debug("sent \"" + TheEvent.Name + "\" event");
        }
    }
    std::cout << "closing " + std::string(__func__) << std::endl;

    _Client.sync_close();
    _Client.clear_con_listeners();
    std::cout << "closed" << std::endl;

}
