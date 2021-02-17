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
    void SetTCPServer(TTCPServer& Server) { mTCPServer = std::ref(Server); }

private:
    TTCPServer& TCPServer() { return mTCPServer->get(); }
    
    TServer& mServer;
    std::optional<std::reference_wrapper<TTCPServer>> mTCPServer { std::nullopt };
    bool mShutdown { false };
    int mInternalPPS { 0 };
};