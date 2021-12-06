#pragma once

#include <thread>

// pure virtual class to be inherited from by classes which intend to be threaded
class IThreaded {
public:
    IThreaded()
        // invokes operator() on this object
        : mThread() { }
    ~IThreaded() noexcept {
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    virtual void Start() final {
        mThread = std::thread([this] { (*this)(); });
    }
    virtual void operator()() = 0;

protected:
    std::thread mThread;
};
