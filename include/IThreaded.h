#pragma once

#include <boost/thread/scoped_thread.hpp>
#include <thread>

// pure virtual class to be inherited from by classes which intend to be threaded
class IThreaded {
public:
    IThreaded()
        // invokes operator() on this object
        : mThread() { }
    virtual ~IThreaded() noexcept {
        mThread.interrupt();
    }

    virtual void Start() final {
        mThread = boost::scoped_thread<>([this] { (*this)(); });
    }
    virtual void operator()() = 0;

protected:
    boost::scoped_thread<> mThread {};
};
