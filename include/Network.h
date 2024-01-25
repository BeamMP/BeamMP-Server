#pragma once

#include "Common.h"
#include "Packet.h"
#include "State.h"
#include "Sync.h"
#include "Transport.h"
#include "Util.h"
#include <boost/asio.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/thread_pool.hpp>
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
    using StrandPtr = std::shared_ptr<boost::asio::strand<ip::tcp::socket::executor_type>>;
    using Ptr = std::shared_ptr<Client>;

    ClientID id;
    Sync<bmp::State> state { bmp::State::None };

    Sync<std::string> name;
    Sync<std::string> role;
    Sync<bool> is_guest;
    Sync<std::unordered_map<std::string /* identifier */, std::string /* value */>> identifiers;
    /// Writes the packet to the TCP stream. Blocks all other writes.
    void tcp_write(bmp::Packet& packet);
    /// Writes the specified to the TCP stream without a header or any metadata - use in
    /// conjunction with something else. Blocks other writes.
    void tcp_write_file_raw(const std::filesystem::path& path);

    Client(ClientID id, class Network& network, ip::tcp::socket&& tcp_socket);
    ~Client();

    ip::tcp::socket& tcp_socket() { return m_tcp_socket; }

    void start_tcp();

    /// Used to associate the udp socket with this client.
    /// This isn't very secure and still allows spoofing of the UDP connection (technically),
    /// but better than simply using the ID like the old protocol.
    const uint64_t udp_magic;

    const ip::udp::endpoint& udp_endpoint() const { return m_udp_endpoint; }
    void set_udp_endpoint(const ip::udp::endpoint& ep) { m_udp_endpoint = ep; }

private:
    /// Call this when the client seems to have timed out. Will send a ping and set a flag.
    /// Returns true if try-again, false if the connection was closed.
    [[nodiscard]] bool handle_timeout();
    bool m_timed_out { false };

    /// Timeout used for typical tcp reads.
    boost::posix_time::milliseconds m_read_timeout { 5000 };
    /// Timeout used for typical tcp writes. Specified in milliseconds per byte.
    /// For example, 10 mbit/s works out to 1250 B/ms, so a value of 1250 here would
    /// cause clients with >10 mbit/s download speed to usually not time out.
    /// This is done because a write is considered completed when all data is written,
    /// and worst-case this could mean that we're limited by their download speed.
    /// We're setting it to 50, which will drop clients who are below a download speed + ping
    /// combination of 0.4 mbit/s.
    double m_write_byte_timeout { 0.01 };
    /// Timeout used for mod download tcp writes.
    /// This is typically orders of magnitude larger
    /// to allow for slow downloads.
    boost::posix_time::milliseconds m_download_write_timeout { 60000 };

    std::mutex m_tcp_read_mtx;
    std::mutex m_tcp_write_mtx;
    std::mutex m_udp_read_mtx;

    ip::tcp::socket m_tcp_socket;

    boost::scoped_thread<> m_tcp_thread;

    std::vector<uint8_t> m_header { bmp::Header::SERIALIZED_SIZE };
    bmp::Packet m_packet {};

    ip::udp::endpoint m_udp_endpoint;

    class Network& m_network;

    StrandPtr m_tcp_strand;
};

struct Vehicle {
    using Ptr = std::shared_ptr<Vehicle>;
    Sync<ClientID> owner;
    Sync<std::vector<uint8_t>> data;

    Vehicle(std::span<uint8_t> raw_data)
        : data(std::vector<uint8_t>(raw_data.begin(), raw_data.end())) {
        reset_status(data.get());
    }

    /// Resets all status fields to zero and reads any statuses present in the data into the fields.
    void reset_status(std::span<const uint8_t> status_data);

    struct Status {
        glm::vec3 rvel {};
        glm::vec4 rot {};
        glm::vec3 vel {};
        glm::vec3 pos {};
        float time {};
    };

    Status get_status();

    void update_status(std::span<const uint8_t> raw_packet);

    const std::vector<uint8_t>& get_raw_status() const { return m_status_data; }

private:
    std::recursive_mutex m_mtx;

    /// Holds pos, rvel, vel, etc. raw, updated every time
    /// such a packet arrives.
    std::vector<uint8_t> m_status_data;

    /// Parses the status_data on request sets needs_refresh = false.
    void refresh_cache(std::unique_lock<std::recursive_mutex>& lock);

    bool m_needs_refresh = false;
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

    void send_to(ClientID id, bmp::Packet& packet);

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

    std::optional<Vehicle::Ptr> get_vehicle(VehicleID id);

    /// Builds the SessionSetup.PlayersInfo json which contains all player info and all vehicles.
    nlohmann::json build_players_info();

    size_t authenticated_client_count() const;

    size_t clients_in_state_count(bmp::State state) const;

    size_t guest_count() const;

    size_t vehicle_count() const;

    /// Creates a Playing state packet from uncompressed data.
    bmp::Packet make_playing_packet(bmp::Purpose purpose, ClientID from_id, VehicleID veh_id, const std::vector<uint8_t>& data);

    /// Sends a <System> or <Server> chat message to all or only one client(s).
    void send_system_chat_message(const std::string& msg, ClientID to = 0xffffffff);

    /// To be called by accept() async handler once an accept() is completed.
    void accept();

    /// Gets the async i/o context of the network - can be used to "schedule" tasks on it.
    boost::asio::thread_pool& context() {
        return m_threadpool;
    }

private:
    void handle_packet(ClientID id, const bmp::Packet& packet);

    /// Reads a packet from the given UDP socket, returning the client's endpoint as an out-argument.
    bmp::Packet udp_read(ip::udp::endpoint& out_ep);
    /// Sends a packet to the specified UDP endpoint via the UDP socket.
    void udp_write(bmp::Packet& packet, const ip::udp::endpoint& ep);

    void udp_read_main();
    void tcp_listen_main();

    /// Handles all packets which are allowed during the Identification state.
    void handle_identification(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client);
    /// Handles all packets which are allowed during the Authentication state.
    void handle_authentication(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client);
    /// Handles all packets which are allowed during the ModDownload state.
    void handle_mod_download(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client);
    /// Handles all packets which are allowed during the SessionSetup state.
    void handle_session_setup(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client);
    /// Handles all packets which are allowed during the Playing state.
    void handle_playing(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client);

    /// On failure, throws an exception with the error for the client.
    static void authenticate_user(const std::string& public_key, std::shared_ptr<Client>& client);

    /// Called by accept() once completed (completion handler).
    void handle_accept(const boost::system::error_code& ec);

    Sync<std::unordered_map<ClientID, Client::Ptr>> m_clients {};
    Sync<std::unordered_map<VehicleID, Vehicle::Ptr>> m_vehicles {};
    Sync<std::unordered_map<uint64_t, ClientID>> m_client_magics {};
    Sync<std::unordered_map<ip::udp::endpoint, ClientID>> m_udp_endpoints {};

    ClientID new_client_id();
    VehicleID new_vehicle_id();

    thread_pool m_threadpool { std::thread::hardware_concurrency() };
    Sync<bool> m_shutdown { false };

    ip::udp::socket m_udp_socket { m_threadpool };

    ip::tcp::socket m_tcp_listener { m_threadpool };
    ip::tcp::acceptor m_tcp_acceptor { m_threadpool };
    /// This socket gets accepted into, and is then moved.
    ip::tcp::socket m_temp_socket { m_threadpool };

    boost::scoped_thread<> m_tcp_listen_thread;
    boost::scoped_thread<> m_udp_read_thread;
};
