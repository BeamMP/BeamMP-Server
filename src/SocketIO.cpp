#include "SocketIO.h"
#include "Common.h"
#include <iostream>

static std::unique_ptr<SocketIO> SocketIOInstance = std::make_unique<SocketIO>();

SocketIO& SocketIO::Get() {
    return *SocketIOInstance;
}

SocketIO::SocketIO() noexcept
    : mThread([this] { ThreadMain(); }) {

    mClient.socket("/" + Application::TSettings().Key)->on("Hello", [&](sio::event&) {
        info("Got 'Hello' from backend socket-io!");
    });

    mClient.set_logs_quiet();
    mClient.set_reconnect_delay(10000);
    mClient.connect("https://backend.beammp.com");

    //mClient.socket()->emit("initConnection", Application::TSettings().Key);
}

SocketIO::~SocketIO() {
    mCloseThread.store(true);
    mThread.join();
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
    if (!mAuthenticated) {
        debug("trying to emit a socket.io event when not yet authenticated");
        return;
    }
    std::string RoomName = RoomNameFromEnum(Room);
    std::string EventName = EventNameFromEnum(Event);
    debug("emitting event \"" + EventName + "\" with data: \"" + Data + "\" in room \"/key/" + RoomName + "\"");
    std::unique_lock Lock(mQueueMutex);
    mQueue.push_back({ RoomName, EventName, Data });
    debug("queue now has " + std::to_string(mQueue.size()) + " events");
}

void SocketIO::ThreadMain() {
    bool FirstTime = true;
    while (!mCloseThread.load()) {
        if (mAuthenticated && FirstTime) {
            FirstTime = false;
        }
        bool empty = false;
        { // queue lock scope
            std::unique_lock Lock(mQueueMutex);
            empty = mQueue.empty();
        } // end queue lock scope
        if (empty) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        } else {
            Event TheEvent;
            { // queue lock scope
                std::unique_lock Lock(mQueueMutex);
                TheEvent = mQueue.front();
                mQueue.pop_front();
            } // end queue lock scope
            debug("sending \"" + TheEvent.Name + "\" event");
            auto Room = "/" + TheEvent.Room;
            mClient.socket()->emit(TheEvent.Name, TheEvent.Data);
            debug("sent \"" + TheEvent.Name + "\" event");
        }
    }
    // using std::cout as this happens during static destruction and the logger might be dead already
    std::cout << "closing " + std::string(__func__) << std::endl;

    mClient.sync_close();
    mClient.clear_con_listeners();

    std::cout << "closed" << std::endl;
}
