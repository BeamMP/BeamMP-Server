#include "Client.h"

#include "CustomAssert.h"
#include "TServer.h"
#include <memory>
#include <optional>

void TClient::DeleteCar(int Ident) {
    // TODO: Send delete packets
    auto VehData = VehicleData.synchronize();
    auto iter = std::find_if(VehData->begin(), VehData->end(), [&](auto& elem) {
        return Ident == elem.ID();
    });
    if (iter != VehData->end()) {
        VehData->erase(iter);
    } else {
        beammp_debug("tried to erase a vehicle that doesn't exist (not an error)");
    }
}

void TClient::ClearCars() {
    VehicleData->clear();
}

int TClient::GetOpenCarID() const {
    int OpenID = 0;
    bool found;
    auto VehData = VehicleData.synchronize();
    do {
        found = true;
        for (auto& v : *VehData) {
            if (v.ID() == OpenID) {
                OpenID++;
                found = false;
            }
        }
    } while (!found);
    return OpenID;
}

void TClient::AddNewCar(int Ident, const std::string& Data) {
    VehicleData->emplace_back(Ident, Data);
}

std::string TClient::GetCarPositionRaw(int Ident) {
    try {
        return VehiclePosition->at(size_t(Ident));
    } catch (const std::out_of_range& oor) {
        beammp_debugf("GetCarPositionRaw failed for id {}, as that car doesn't exist on client id {}: {}", Ident, int(ID), oor.what());
        return "";
    }
}

void TClient::Disconnect(std::string_view Reason) {
    auto LockedSocket = TCPSocket.synchronize();
    beammp_debugf("Disconnecting client {} for reason: {}", int(ID), Reason);
    boost::system::error_code ec;
    LockedSocket->shutdown(socket_base::shutdown_both, ec);
    if (ec) {
        beammp_debugf("Failed to shutdown client socket: {}", ec.message());
    }
    LockedSocket->close(ec);
    if (ec) {
        beammp_debugf("Failed to close client socket: {}", ec.message());
    }
}

void TClient::SetCarPosition(int Ident, const std::string& Data) {
    // ugly but this is c++ so
    VehiclePosition->operator[](size_t(Ident)) = Data;
}

std::string TClient::GetCarData(int Ident) {
    { // lock
        auto Lock = VehicleData.synchronize();
        for (auto& v : *Lock) {
            if (v.ID() == Ident) {
                return v.Data();
            }
        }
    } // unlock
    DeleteCar(Ident);
    return "";
}

void TClient::SetCarData(int Ident, const std::string& Data) {
    { // lock
        auto Lock = VehicleData.synchronize();
        for (auto& v : *Lock) {
            if (v.ID() == Ident) {
                v.SetData(Data);
                return;
            }
        }
    } // unlock
    DeleteCar(Ident);
}

int TClient::GetCarCount() const {
    return int(VehicleData->size());
}

TServer& TClient::Server() const {
    return mServer;
}

void TClient::EnqueuePacket(const std::vector<uint8_t>& Packet) {
    MissedPacketsQueue->push(Packet);
}

TClient::TClient(TServer& Server, ip::tcp::socket&& Socket)
    : TCPSocket(std::move(Socket))
    , DownSocket(ip::tcp::socket(Server.IoCtx()))
    , LastPingTime(std::chrono::high_resolution_clock::now())
    , mServer(Server) {
}

TClient::~TClient() {
    beammp_debugf("client destroyed: {} ('{}')", ID.get(), Name.get());
}

void TClient::UpdatePingTime() {
    LastPingTime = std::chrono::high_resolution_clock::now();
}
int TClient::SecondsSinceLastPing() {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - LastPingTime.get())
                       .count();
    return int(seconds);
}

std::optional<std::shared_ptr<TClient>> GetClient(TServer& Server, int ID) {
    std::optional<std::shared_ptr<TClient>> MaybeClient { std::nullopt };
    Server.ForEachClient([ID, &MaybeClient](const auto& Client) -> bool {
        if (Client->ID.get() == ID) {
            MaybeClient = Client;
            return false;
        }
        } else {
            beammp_debugf("Found an expired client while looking for id {}", ID);
        return true;
    });
    return MaybeClient;
}
void TClient::SetIdentifier(const std::string& key, const std::string& value) {
    // I know this is bad, but what can ya do
    Identifiers->operator[](key) = value;
}
