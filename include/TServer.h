#pragma once

#include "IThreaded.h"
#include "RWMutex.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>

class TClient;
class TUDPServer;
class TTCPServer;
class TPPSMonitor;

class TServer final {
public:
    using TClientSet = std::unordered_set<std::shared_ptr<TClient>>;

    TServer(int argc, char** argv);

    void InsertClient(std::shared_ptr<TClient> Ptr);
    std::weak_ptr<TClient> InsertNewClient();
    void RemoveClient(std::weak_ptr<TClient>);
    // in Fn, return true to continue, return false to break
    void ForEachClient(const std::function<bool(std::weak_ptr<TClient>)>& Fn);
    size_t ClientCount() const;

    static void GlobalParser(std::weak_ptr<TClient> Client, std::string Packet, TPPSMonitor& PPSMonitor, TUDPServer& UDPServer, TTCPServer& TCPServer);
    static void HandleEvent(TClient& c, const std::string& Data);

private:
    TClientSet mClients;
    mutable RWMutex mClientsMutex;
    static void ParseVehicle(TClient& c, const std::string& Pckt, TTCPServer& TCPServer, TUDPServer& UDPServer);
    static void Apply(TClient& c, int VID, const std::string& pckt);
};
