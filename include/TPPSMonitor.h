#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "TServer.h"

class TPPSMonitor : public IThreaded {
public:
    TPPSMonitor(TServer& Server);

    void operator()() override;

    void SetInternalPPS(int NewPPS) { mInternalPPS = NewPPS; }
    void IncrementInternalPPS() { ++mInternalPPS; }
    [[nodiscard]] int InternalPPS() const { return mInternalPPS; }

private:
    TServer& mServer;
    bool mShutdown { false };
    int mInternalPPS { 0 };
};