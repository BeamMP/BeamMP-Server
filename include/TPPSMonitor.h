#pragma once

#include "Common.h"
#include "TServer.h"

class TPPSMonitor : public IThreaded {
public:
    explicit TPPSMonitor(TServer& Server);

    void operator()() override;

    void SetInternalPPS(int NewPPS) { mInternalPPS = NewPPS; }
    void IncrementInternalPPS() { ++mInternalPPS; }
    [[nodiscard]] int InternalPPS() const { return mInternalPPS; }

private:
    TServer& mServer;
    bool mShutdown { false };
    int mInternalPPS { 0 };
};