#include "SocketIO.h"
#include "Common.h"
#include <iostream>


//TODO Default disabled with config option
static std::unique_ptr<SocketIO> SocketIOInstance = std::make_unique<SocketIO>();

SocketIO& SocketIO::Get() {
    return *SocketIOInstance;
}

SocketIO::SocketIO() noexcept
    : mThread([this] { ThreadMain(); }) {

    mClient.socket()->on("network", [&](sio::event&e) {
        if(e.get_message()->get_string() == "Welcome"){
            info("SocketIO Authenticated!");
            mAuthenticated = true;
        }
    });

    mClient.socket()->on("welcome", [&](sio::event&) {
        info("Got welcome from backend! Authenticating SocketIO...");
        mClient.socket()->emit("onInitConnection", Application::Settings.Key);
    });

    mClient.set_logs_quiet();
    mClient.set_reconnect_delay(10000);
    mClient.connect(Application::GetBackendUrlForSocketIO());
}

SocketIO::~SocketIO() {
    mCloseThread.store(true);
    mThread.join();
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

void SocketIO::Emit(SocketIOEvent Event, const std::string& Data) {
    if (!mAuthenticated) {
        debug("trying to emit a socket.io event when not yet authenticated");
        return;
    }
    std::string EventName = EventNameFromEnum(Event);
    debug("emitting event \"" + EventName + "\" with data: \"" + Data);
    std::unique_lock Lock(mQueueMutex);
    mQueue.push_back({EventName, Data });
    debug("queue now has " + std::to_string(mQueue.size()) + " events");
}

void SocketIO::ThreadMain() {
    while (!mCloseThread.load()) {
        bool empty;
        { // queue lock scope
            std::unique_lock Lock(mQueueMutex);
            empty = mQueue.empty();
        } // end queue lock scope
        if (empty || !mClient.opened()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else {
            Event TheEvent;
            { // queue lock scope
                std::unique_lock Lock(mQueueMutex);
                TheEvent = mQueue.front();
                mQueue.pop_front();
            } // end queue lock scope
            debug("sending \"" + TheEvent.Name + "\" event");
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
