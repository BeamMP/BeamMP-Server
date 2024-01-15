#pragma once

#include "State.h"
#include "Sync.h"
#include "Transport.h"
#include <boost/asio.hpp>
#include <boost/thread/scoped_thread.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

using ClientID = uint32_t;
using VehicleID = uint16_t;

using namespace boost::asio;

struct Packet {
    bmp::Purpose purpose;
    bmp::Flags flags;
    std::vector<uint8_t> data;

    bmp::Header header() const;
};

struct Client {
    using Ptr = std::shared_ptr<Client>;
    ClientID id;
    bmp::State state { bmp::State::None };

    /// Reads a single packet from the TCP stream. Blocks all other reads (not writes).
    Packet tcp_read();
    /// Writes the packet to the TCP stream. Blocks all other writes.
    void tcp_write(const Packet& packet);
    /// Writes the specified to the TCP stream without a header or any metadata - use in
    /// conjunction with something else. Blocks other writes.
    void tcp_write_file_raw(const std::filesystem::path& path);

    Client(ClientID id, class Network& network, ip::tcp::socket&& tcp_socket);
    ~Client();

    ip::tcp::socket& tcp_socket() { return m_tcp_socket; }

    [[nodiscard]] const ip::udp::endpoint& udp_endpoint() const { return m_udp_ep; }
    void set_udp_endpoint(const ip::udp::endpoint& ep) { m_udp_ep = ep; }

    void start_tcp();

private:

    void tcp_main();

    std::mutex m_tcp_read_mtx;
    std::mutex m_tcp_write_mtx;
    std::mutex m_udp_read_mtx;

    ip::udp::endpoint m_udp_ep;
    ip::tcp::socket m_tcp_socket;

    boost::scoped_thread<> m_tcp_thread;

    class Network& m_network;
};

struct Vehicle {
    using Ptr = std::shared_ptr<Vehicle>;
    ClientID owner;
    std::vector<uint8_t> data;
};

class Network {
public:
    Network();
    ~Network();

    /// Reads a packet from the given UDP socket, returning the client's endpoint as an out-argument.
    Packet udp_read(ip::udp::endpoint& out_ep);
    /// Sends a packet to the specified UDP endpoint via the UDP socket.
    void udp_write(const Packet& packet, const ip::udp::endpoint& to_ep);

    void disconnect(ClientID id, const std::string& msg);

    void handle_packet(ClientID i, const Packet& packet);

private:
    void udp_read_main();
    void tcp_listen_main();

    Sync<std::unordered_map<ClientID, Client::Ptr>> m_clients {};
    Sync<std::unordered_map<VehicleID, Vehicle::Ptr>> m_vehicles {};

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
    void handle_identification(ClientID id, const Packet& packet, std::shared_ptr<Client>& client);
};
