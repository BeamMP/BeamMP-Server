#include "Network.h"
#include "ClientInfo.h"
#include "Common.h"
#include "Environment.h"
#include "ProtocolVersion.h"
#include "ServerInfo.h"

#if defined(BEAMMP_LINUX)
#include <cerrno>
#include <cstring>
#include <sys/sendfile.h>
#include <unistd.h>
#elif defined(BEAMMP_WINDOWS)
#include <boost/iostreams/device/mapped_file.hpp>
#endif

Packet Client::tcp_read() {
    std::unique_lock lock(m_tcp_read_mtx);
    Packet packet {};
    std::vector<uint8_t> header_buffer(bmp::Header::SERIALIZED_SIZE);
    read(m_tcp_socket, buffer(header_buffer));
    bmp::Header hdr {};
    hdr.deserialize_from(header_buffer);
    // vector eaten up by now, recv again
    packet.data.resize(hdr.size);
    read(m_tcp_socket, buffer(packet.data));
    packet.purpose = hdr.purpose;
    packet.flags = hdr.flags;
    return packet;
}

void Client::tcp_write(const Packet& packet) {
    std::unique_lock lock(m_tcp_write_mtx);
    auto header = packet.header();
    std::vector<uint8_t> header_data(bmp::Header::SERIALIZED_SIZE);
    if (header.flags != bmp::Flags::None) {
        beammp_errorf("Flags are not implemented");
    }
    header.serialize_to(header_data);
    write(m_tcp_socket, buffer(header_data));
    write(m_tcp_socket, buffer(packet.data));
}

void Client::tcp_write_file_raw(const std::filesystem::path& path) {
    std::unique_lock lock(m_tcp_write_mtx);
#if defined(BEAMMP_LINUX)
    // sendfile
    auto size = std::filesystem::file_size(path);
    auto in_fd = ::open(path.generic_string().c_str(), O_RDONLY);
    if (in_fd == -1) {
        throw std::runtime_error(fmt::format("IO error opening '{}': {}", path.generic_string(), std::strerror(errno)));
    }
    int out_fd = m_tcp_socket.native_handle();
    auto n = sendfile(out_fd, in_fd, nullptr /* offset */, size);
    ::close(in_fd);
    if (n == -1) {
        throw std::runtime_error(fmt::format("Failed sending '{}' to client via sendfile(): {}", path.generic_string(), std::strerror(errno)));
    }
#else
    // TODO: Use TransmitFile on Windows for better performance
    // primitive implementation using a memory-mapped file
    boost::iostreams::mapped_file f(path.generic_string(), boost::iostreams::mapped_file::mapmode::readonly);
    write(m_tcp_socket, buffer(f.data(), f.size()));
#endif
}

Client::~Client() {
    beammp_debugf("Client {} shutting down", id);
    m_tcp_socket.shutdown(boost::asio::socket_base::shutdown_receive);
    m_tcp_thread.interrupt();
    beammp_debugf("Client {} shut down", id);
}

Client::Client(ClientID id, Network& network, ip::tcp::socket&& tcp_socket)
    : id(id)
    , m_tcp_socket(std::forward<ip::tcp::socket&&>(tcp_socket))
    , m_network(network) {
    beammp_debugf("Client {} created", id);
}

void Client::start_tcp() {
    m_tcp_thread = boost::scoped_thread<>(&Client::tcp_main, this);
}

void Client::tcp_main() {
    beammp_debugf("TCP thread started for client {}", id);
    try {
        while (true) {
            auto packet = tcp_read();
            m_network.handle_packet(id, packet);
        }
    } catch (const std::exception& e) {
        beammp_errorf("Error in tcp loop of client {}: {}", id, e.what());
        m_network.disconnect(id, "error in tcp loop");
    }
    beammp_debugf("TCP thread stopped for client {}", id);
}

bmp::Header Packet::header() const {
    return {
        .purpose = purpose,
        .flags = bmp::Flags::None,
        .rsv = 0,
        .size = static_cast<uint32_t>(data.size()),
    };
}

Packet Network::udp_read(ip::udp::endpoint& out_ep) {
    // maximum we can ever expect from udp
    static thread_local std::vector<uint8_t> s_buffer(std::numeric_limits<uint16_t>::max());
    m_udp_socket.receive_from(buffer(s_buffer), out_ep, {});
    Packet packet;
    bmp::Header header {};
    auto offset = header.deserialize_from(s_buffer);
    if (header.flags != bmp::Flags::None) {
        beammp_errorf("Flags are not implemented");
        return {};
    }
    return packet;
}

void Network::udp_write(const Packet& packet, const ip::udp::endpoint& to_ep) {
    auto header = packet.header();
    std::vector<uint8_t> data(header.size + bmp::Header::SERIALIZED_SIZE);
    auto offset = header.serialize_to(data);
    std::copy(packet.data.begin(), packet.data.end(), data.begin() + static_cast<long>(offset));
    m_udp_socket.send_to(buffer(data), to_ep, {});
}

Network::Network()
    : m_tcp_listen_thread(&Network::tcp_listen_main, this)
    , m_udp_read_thread(&Network::udp_read_main, this)
    , m_threadpool(std::thread::hardware_concurrency()) {
    Application::RegisterShutdownHandler([this] {
        *m_shutdown = true;
        m_tcp_listen_thread.interrupt();
        m_udp_read_thread.interrupt();
    });
}

Network::~Network() {
    *m_shutdown = true;
}

void Network::tcp_listen_main() {
    ip::tcp::endpoint listen_ep(ip::address::from_string("0.0.0.0"), static_cast<uint16_t>(Application::Settings.Port));
    ip::tcp::socket listener(m_threadpool);
    boost::system::error_code ec;
    listener.open(listen_ep.protocol(), ec);
    if (ec) {
        beammp_errorf("Failed to open socket: {}", ec.message());
        return;
    }
    socket_base::linger linger_opt {};
    linger_opt.enabled(false);
    listener.set_option(linger_opt, ec);
    if (ec) {
        beammp_errorf("Failed to set up listening socket to not linger / reuse address. "
                      "This may cause the socket to refuse to bind(). Error: {}",
            ec.message());
    }

    ip::tcp::acceptor acceptor(m_threadpool, listen_ep);
    acceptor.listen(socket_base::max_listen_connections, ec);
    if (ec) {
        beammp_errorf("listen() failed, which is needed for the server to operate. "
                      "Shutting down. Error: {}",
            ec.message());
        Application::GracefullyShutdown();
    }
    while (!*m_shutdown) {
        auto new_socket = acceptor.accept();
        if (ec) {
            beammp_errorf("Failed to accept client: {}", ec.message());
            continue;
        }
        // TODO: Remove log
        beammp_debugf("New connection from {}", new_socket.remote_endpoint().address().to_string(), new_socket.remote_endpoint().port());
        auto new_id = new_client_id();
        std::shared_ptr<Client> new_client(std::make_shared<Client>(new_id, *this, std::move(new_socket)));
        m_clients->emplace(new_id, new_client);
    }
}

void Network::udp_read_main() {
    while (!*m_shutdown) {
    }
}

void Network::disconnect(ClientID id, const std::string& msg) {
    beammp_infof("Disconnecting client {}: {}", id, msg);
    m_clients->erase(id);
}
void Network::handle_packet(ClientID id, const Packet& packet) {
    std::shared_ptr<Client> client;
    {
        auto clients = m_clients.synchronize();
        if (!clients->contains(id)) {
            beammp_warnf("Tried to handle packet for client {} who is already disconnected", id);
            return;
        }
        client = clients->at(id);
    }
    switch (client->state) {
    case bmp::State::None:
        // move to identification
        client->state = bmp::State::Identification;
        // and fall through
        [[fallthrough]];
    case bmp::State::Identification:
        switch (packet.purpose) {
        case bmp::Purpose::ProtocolVersion: {
            struct bmp::ProtocolVersion protocol_version { };
            protocol_version.deserialize_from(packet.data);
            if (protocol_version.version.major != 1) {
                beammp_debugf("{}: Protocol version bad", id);
                // version bad
                Packet protocol_v_bad_packet {
                    .purpose = bmp::ProtocolVersionBad,
                };
                client->tcp_write(protocol_v_bad_packet);
                disconnect(id, fmt::format("bad protocol version: {}.{}.{}", protocol_version.version.major, protocol_version.version.minor, protocol_version.version.patch));
            } else {
                beammp_debugf("{}: Protocol version ok", id);
                // version ok
                Packet protocol_v_ok_packet {
                    .purpose = bmp::ProtocolVersionOk,
                };
                client->tcp_write(protocol_v_ok_packet);
            }
            break;
        }
        case bmp::Purpose::ClientInfo: {
            struct bmp::ClientInfo cinfo { };
            cinfo.deserialize_from(packet.data);
            beammp_debugf("{} is running game version: v{}.{}.{}, mod version: v{}.{}.{}, client implementation '{}' v{}.{}.{}",
                id,
                cinfo.game_version.major,
                cinfo.game_version.minor,
                cinfo.game_version.patch,
                cinfo.mod_version.major,
                cinfo.mod_version.minor,
                cinfo.mod_version.patch,
                cinfo.implementation.value,
                cinfo.program_version.major,
                cinfo.program_version.minor,
                cinfo.program_version.patch);
            break;
        }
        default:
            beammp_errorf("Got 0x{:x} in state {}. This is not allowed disconnecting the client", uint16_t(packet.purpose), int(client->state));
            disconnect(id, "invalid purpose in current state");
            return;
        }
        break;
    case bmp::State::Authentication:
        break;
    case bmp::State::ModDownload:
        break;
    case bmp::State::SessionSetup:
        break;
    case bmp::State::Playing:
        break;
    case bmp::State::Leaving:
        break;
    }
}
