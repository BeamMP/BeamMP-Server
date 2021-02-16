#pragma once

#include "IThreaded.h"
#include "Common.h"

class THeartbeatThread : public IThreaded {
public:
    THeartbeatThread();
    void operator()() override;
};