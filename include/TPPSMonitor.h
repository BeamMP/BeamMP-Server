#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "TServer.h"

class TPPSMonitor : public IThreaded {
public:
    TPPSMonitor(TServer& Server);

    void operator()() override;

private:

    TServer& mServer;
    bool mShutdown { false };
    int mInternalPPS { 0 };
};