#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "RWMutex.h"

class TClient;

class TServer final {
public:
    using TClientSet = std::unordered_set<std::shared_ptr<TClient>>;

    TServer(int argc, char** argv);

    std::weak_ptr<TClient> InsertNewClient();
    void RemoveClient(std::weak_ptr<TClient>);
    void ForEachClient(std::function<bool(std::weak_ptr<TClient>)>);
    size_t ClientCount() const;

private:
    TClientSet _Clients;
    mutable RWMutex _ClientsMutex;
};
