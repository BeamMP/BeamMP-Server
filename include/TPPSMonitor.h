#pragma once

#include "Common.h"
#include "TServer.h"
#include <optional>

class TNetwork;

class TPPSMonitor : public IThreaded {
public:
    explicit TPPSMonitor(TServer& Server);

    void operator()() override;

    void SetInternalPPS(int NewPPS) { mInternalPPS = NewPPS; }
    void IncrementInternalPPS() { ++mInternalPPS; }
    [[nodiscard]] int InternalPPS() const { return mInternalPPS; }
    void SetNetwork(TNetwork& Server) { mNetwork = std::ref(Server); }

private:
    TNetwork& Network() { return mNetwork->get(); }

    TServer& mServer;
    std::optional<std::reference_wrapper<TNetwork>> mNetwork { std::nullopt };
    int mInternalPPS { 0 };
};
