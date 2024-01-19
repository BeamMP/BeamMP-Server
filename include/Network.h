#pragma once

#include "Common.h"
#include "Packet.h"
#include "State.h"
#include "Sync.h"
#include "Transport.h"
#include <boost/asio.hpp>
#include <boost/thread/scoped_thread.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <cstdint>
#include <filesystem>
#include <glm/detail/qualifier.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

using ClientID = uint32_t;
using VehicleID = uint16_t;

using namespace boost::asio;

struct Client {
    using Ptr = std::shared_ptr<Client>;
    ClientID id;
    Sync<bmp::State> state { bmp::State::None };

    Sync<std::string> name;
    Sync<std::string> role;
    Sync<bool> is_guest;
    Sync<std::unordered_map<std::string /* identifier */, std::string /* value */>> identifiers;

    /// Reads a single packet from the TCP stream. Blocks all other reads (not writes).
    bmp::Packet tcp_read();
    /// Writes the packet to the TCP stream. Blocks all other writes.
    void tcp_write(bmp::Packet& packet);
    /// Writes the specified to the TCP stream without a header or any metadata - use in
    /// conjunction with something else. Blocks other writes.
    void tcp_write_file_raw(const std::filesystem::path& path);

    Client(ClientID id, class Network& network, ip::tcp::socket&& tcp_sockem_udp_endpointst);
    ~Client();

    ip::tcp::socket& tcp_socket() { return m_tcp_socket; }

    void start_tcp();

    /// Used to associate the udp socket with this client.
    /// This isn't very secure and still allows spoofing of the UDP connection (technically),
    /// but better than simply using the ID like the old protocol.
    const uint64_t udp_magic;

private:
    void tcp_main();

    std::mutex m_tcp_read_mtx;
    std::mutex m_tcp_write_mtx;
    std::mutex m_udp_read_mtx;

    ip::tcp::socket m_tcp_socket;

    boost::scoped_thread<> m_tcp_thread;

    class Network& m_network;
};

struct Vehicle {
    using Ptr = std::shared_ptr<Vehicle>;
    Sync<ClientID> owner;
    Sync<std::vector<uint8_t>> data;

    struct Status {
        glm::vec3 rvel {};
        glm::vec4 rot {};
        glm::vec3 vel {};
        glm::vec3 pos {};
        float time {};
    };

    Status get_status() {
        std::unique_lock lock(m_mtx);
        refresh_cache(lock);
        return {
            .rvel = m_rvel,
            .rot = m_rot,
            .vel = m_vel,
            .pos = m_pos,
            .time = m_time,
        };
    }

    void update_status(const std::vector<uint8_t>& raw_packet) {
        std::unique_lock lock(m_mtx);
        m_needs_refresh = true;
        m_status_data = raw_packet;
    }

private:
    std::recursive_mutex m_mtx;

    /// Holds pos, rvel, vel, etc. raw, updated every time
    /// such a packet arrives.
    std::vector<uint8_t> m_status_data;

    /// Parses the status_data on request sets needs_refresh = false.
    void refresh_cache(std::unique_lock<std::recursive_mutex>& lock);

    bool m_needs_refresh = true;
    glm::vec3 m_rvel {};
    glm::vec4 m_rot {};
    glm::vec3 m_vel {};
    glm::vec3 m_pos {};
    float m_time {};
};

class Network {
public:
    Network();
    ~Network();

    friend Client;

    void disconnect(ClientID id, const std::string& msg);

    void send_to(ClientID id, const bmp::Packet& packet);

    /// Returns a map of <id, client> containing only clients which are
    /// fully connected, i.e. who have mods downloaded and everything spawned in.
    /// If you're unsure which to use, use this one.
    std::unordered_map<ClientID, Client::Ptr> playing_clients() const;
    /// Returns a map of <id, client> containing only clients who are authenticated.
    std::unordered_map<ClientID, Client::Ptr> authenticated_clients() const;
    /// Returns all clients, including non-authenticated clients. Use only for debugging,
    /// information, stats, status.
    std::unordered_map<ClientID, Client::Ptr> all_clients() const;

    std::optional<Client::Ptr> get_client(ClientID id, bmp::State min_state) const;

    std::unordered_map<VehicleID, Vehicle::Ptr> get_vehicles_owned_by(ClientID id);

    std::optional<Vehicle::Ptr> get_vehicle(VehicleID id) {
        auto vehicles = m_vehicles.synchronize();
        if (vehicles->contains(id)) {
            return vehicles->at(id);
        } else {
            return std::nullopt;
        }
    }

    size_t authenticated_client_count() const;

    size_t clients_in_state_count(bmp::State state) const;

    size_t guest_count() const;

    size_t vehicle_count() const;

private:
    void handle_packet(ClientID id, const bmp::Packet& packet);

    /// Reads a packet from the given UDP socket, returning the client's endpoint as an out-argument.
    bmp::Packet udp_read(ip::udp::endpoint& out_ep);
    /// Sends a packet to the specified UDP endpoint via the UDP socket.
    void udp_write(bmp::Packet& packet, const ip::udp::endpoint& to_ep);

    void udp_read_main();
    void tcp_listen_main();

    Sync<std::unordered_map<ClientID, Client::Ptr>> m_clients {};
    Sync<std::unordered_map<VehicleID, Vehicle::Ptr>> m_vehicles {};
    Sync<std::unordered_map<uint64_t, ClientID>> m_client_magics {};
    Sync<std::unordered_map<ip::udp::endpoint, ClientID>> m_udp_endpoints {};

    ClientID new_client_id() {
        static Sync<ClientID> s_id { 0 };
        auto id = s_id.synchronize();
        ClientID new_id = *id;
        *id += 1;
        return new_id;
    }

    boost::scoped_thread<> m_tcp_listen_thread;
    boost::scoped_thread<> m_udp_read_thread;

    io_context m_io {};
    thread_pool m_threadpool {};
    Sync<bool> m_shutdown { false };
    ip::udp::socket m_udp_socket { m_io };

    void handle_identification(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client);

    void handle_authentication(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client);

    /// On failure, throws an exception with the error for the client.
    static void authenticate_user(const std::string& public_key, std::shared_ptr<Client>& client);
};
