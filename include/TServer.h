#pragma once

#include "IThreaded.h"
#include "RWMutex.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>

class TClient;
class TNetwork;
class TPPSMonitor;

class TServer final {
public:
    using TClientSet = std::unordered_set<std::shared_ptr<TClient>>;

    TServer(int argc, char** argv);

    void InsertClient(const std::shared_ptr<TClient>& Ptr);
    std::weak_ptr<TClient> InsertNewClient();
    void RemoveClient(const std::weak_ptr<TClient>&);
    // in Fn, return true to continue, return false to break
    void ForEachClient(const std::function<bool(std::weak_ptr<TClient>)>& Fn);
    size_t ClientCount() const;

    static void GlobalParser(const std::weak_ptr<TClient>& Client, std::string Packet, TPPSMonitor& PPSMonitor, TNetwork& Network);
    static void HandleEvent(TClient& c, const std::string& Data);

private:
    TClientSet mClients;
    mutable RWMutex mClientsMutex;
    static void ParseVehicle(TClient& c, const std::string& Pckt, TNetwork& Network);
    static void Apply(TClient& c, int VID, const std::string& pckt);
};
