#pragma once

#include "State.h"
#include "Sync.h"
#include "Transport.h"
#include <boost/asio.hpp>
#include <cstdint>
#include <memory>
#include <filesystem>
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
    bmp::State state;

    Packet tcp_read(boost::system::error_code& ec);
    void tcp_write(const Packet& packet, boost::system::error_code& ec);
    void tcp_write_file_raw(const std::filesystem::path& path, boost::system::error_code& ec);
    Packet udp_read(boost::system::error_code& ec, ip::udp::socket& socket);
    void udp_write(const Packet& packet, ip::udp::socket& socket, boost::system::error_code& ec);

    Client(ip::udp::endpoint& ep, ip::tcp::socket&& socket);
    ~Client();

private:
    std::mutex m_tcp_read_mtx;
    std::mutex m_tcp_write_mtx;
    std::mutex m_udp_read_mtx;

    ip::udp::endpoint m_udp_ep;
    ip::tcp::socket m_tcp_socket;
};

struct Vehicle {
    using Ptr = std::shared_ptr<Vehicle>;
    ClientID owner;
    std::vector<uint8_t> data;
};

class Network {
public:


private:
    Sync<std::unordered_map<ClientID, Client::Ptr>> m_clients;
    Sync<std::unordered_map<VehicleID, Vehicle::Ptr>> m_vehicles;
    boost::asio::io_context m_io;
};

