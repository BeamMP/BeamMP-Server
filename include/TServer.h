#pragma once

#include "IThreaded.h"
#include "IterationDecision.h"
#include "RWMutex.h"
#include "TScopedTimer.h"
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_set>

#include "BoostAliases.h"

class TClient;
class TNetwork;
class TPPSMonitor;

// clang-format doesn't know how to deal with concepts
// clang-format off
template <typename FnT>
concept ForEachHandlerWithDecision = requires(FnT Fn, const std::shared_ptr<TClient>& Ptr) {
    requires std::invocable<FnT, const std::shared_ptr<TClient>&> ;
    { std::invoke(Fn, Ptr) } -> std::convertible_to<IterationDecision>;
};

template <typename FnT>
concept ForEachHandler = requires(FnT Fn, const std::shared_ptr<TClient>& Ptr) {
    requires std::invocable <FnT, const std::shared_ptr<TClient>&> ;
    { std::invoke(Fn, Ptr) } -> std::same_as<void>;
};
// clang-format on

class TServer final {
public:
    using TClientSet = std::unordered_set<std::shared_ptr<TClient>>;

    TServer(const std::vector<std::string_view>& Arguments);

    void InsertClient(const std::shared_ptr<TClient>& Ptr);
    void RemoveClient(const std::weak_ptr<TClient>&);
    // in Fn, return true to continue, return false to break
    [[deprecated("use ForEachClient instead")]] void ForEachClientWeak(const std::function<bool(std::weak_ptr<TClient>)>& Fn);
    // in Fn, return Break or Continue
    template <ForEachHandlerWithDecision FnT>
    void ForEachClient(FnT Fn) {
        decltype(mClients) Clients;
        {
            ReadLock lock(mClientsMutex);
            Clients = mClients;
        }
        for (auto& Client : Clients) {
            IterationDecision Decision = std::invoke(Fn, Client);
            if (Decision == IterationDecision::Break) {
                break;
            }
        }
    }
    template <ForEachHandler FnT>
    void ForEachClient(FnT Fn) {
        decltype(mClients) Clients;
        {
            ReadLock lock(mClientsMutex);
            Clients = mClients;
        }
        for (auto& Client : Clients) {
            std::invoke(Fn, Client);
        }
    }
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
