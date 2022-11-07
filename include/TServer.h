#pragma once

#include "IThreaded.h"
#include "RWMutex.h"
#include "TScopedTimer.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "BoostAliases.h"

class TClient;
class TNetwork;
class TPPSMonitor;

class TServer final {
public:
    using TClientSet = std::unordered_set<std::shared_ptr<TClient>>;

    TServer(const std::vector<std::string_view>& Arguments);

    void InsertClient(const std::shared_ptr<TClient>& Ptr);
    void RemoveClient(const std::weak_ptr<TClient>&);
    // in Fn, return true to continue, return false to break
    void ForEachClient(const std::function<bool(std::weak_ptr<TClient>)>& Fn);
    size_t ClientCount() const;

    static void GlobalParser(const std::weak_ptr<TClient>& Client, std::vector<uint8_t>&& Packet, TNetwork& Network);
    static void HandleEvent(TClient& c, const std::string& Data);
    RWMutex& GetClientMutex() const { return mClientsMutex; }

    const TScopedTimer UptimeTimer;

    // asio io context
    io_context& IoCtx() { return mIoCtx; }

private:
    io_context mIoCtx {};
    TClientSet mClients;
    mutable RWMutex mClientsMutex;
    static void ParseVehicle(TClient& c, const std::string& Pckt, TNetwork& Network);
    static bool ShouldSpawn(TClient& c, const std::string& CarJson, int ID);
    static bool IsUnicycle(TClient& c, const std::string& CarJson);
    static void Apply(TClient& c, int VID, const std::string& pckt);
    static bool HandlePosition(TClient& c, const std::string& PacketStr);
    static bool HandleVehicleUpdate(const std::string& PacketStr, const int playerID);
};

struct BufferView {
    uint8_t* Data { nullptr };
    size_t Size { 0 };
    const uint8_t* data() const { return Data; }
    uint8_t* data() { return Data; }
    size_t size() const { return Size; }
};
