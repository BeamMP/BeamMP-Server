#include "Network.h"
#include "ClientInfo.h"
#include "Common.h"
#include "Compression.h"
#include "Environment.h"
#include "Http.h"
#include "LuaAPI.h"
#include "Packet.h"
#include "ProtocolVersion.h"
#include "ServerInfo.h"
#include "TLuaEngine.h"
#include "Util.h"
#include <boost/thread/synchronized_value.hpp>
#include <cstdlib>
#include <nlohmann/json.hpp>

#if defined(BEAMMP_LINUX)
#include <cerrno>
#include <cstring>
#include <sys/sendfile.h>
#include <unistd.h>
#elif defined(BEAMMP_WINDOWS)
#include <boost/iostreams/device/mapped_file.hpp>
#endif

#include <doctest/doctest.h>

bmp::Packet Client::tcp_read() {
    std::unique_lock lock(m_tcp_read_mtx);
    bmp::Packet packet {};
    std::vector<uint8_t> header_buffer(bmp::Header::SERIALIZED_SIZE);
    read(m_tcp_socket, buffer(header_buffer));
    bmp::Header hdr {};
    hdr.deserialize_from(header_buffer);
    // vector eaten up by now, recv again
    packet.raw_data.resize(hdr.size);
    read(m_tcp_socket, buffer(packet.raw_data));
    packet.purpose = hdr.purpose;
    packet.flags = hdr.flags;
    return packet;
}

void Client::tcp_write(bmp::Packet& packet) {
    beammp_tracef("Sending 0x{:x} to {}", int(packet.purpose), id);
    // acquire a lock to avoid writing a header, then being interrupted by another write
    std::unique_lock lock(m_tcp_write_mtx);
    // finalize the packet (compress etc) and produce header
    auto header = packet.finalize();
    // serialize header
    std::vector<uint8_t> header_data(bmp::Header::SERIALIZED_SIZE);
    if (header.flags != bmp::Flags::None) {
        beammp_errorf("Flags are not implemented");
    }
    header.serialize_to(header_data);
    // write header and packet data
    write(m_tcp_socket, buffer(header_data));
    write(m_tcp_socket, buffer(packet.raw_data));
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
    , udp_magic(id ^ uint64_t(std::rand()) ^ uint64_t(this))
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

bmp::Packet Network::udp_read(ip::udp::endpoint& out_ep) {
    // maximum we can ever expect from udp
    static thread_local std::vector<uint8_t> s_buffer(std::numeric_limits<uint16_t>::max());
    m_udp_socket.receive_from(buffer(s_buffer), out_ep, {});
    bmp::Packet packet;
    bmp::Header header {};
    auto offset = header.deserialize_from(s_buffer);
    if (header.flags != bmp::Flags::None) {
        beammp_errorf("Flags are not implemented");
        return {};
    }
    packet.raw_data.resize(header.size);
    std::copy(s_buffer.begin() + offset, s_buffer.begin() + offset + header.size, packet.raw_data.begin());
    return packet;
}

void Network::udp_write(bmp::Packet& packet, const ip::udp::endpoint& to_ep) {
    auto header = packet.finalize();
    std::vector<uint8_t> data(header.size + bmp::Header::SERIALIZED_SIZE);
    auto offset = header.serialize_to(data);
    std::copy(packet.raw_data.begin(), packet.raw_data.end(), data.begin() + static_cast<long>(offset));
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
    m_udp_socket = ip::udp::socket(m_io, ip::udp::endpoint(ip::udp::v4(), Application::Settings.Port));
    while (!*m_shutdown) {
        try {
            ip::udp::endpoint ep;
            auto packet = udp_read(ep);
            // special case for new udp connections, only happens once
            if (packet.purpose == bmp::Purpose::StartUDP) [[unlikely]] {
                auto all = boost::synchronize(m_clients, m_udp_endpoints, m_client_magics);
                auto& clients = std::get<0>(all);
                auto& endpoints = std::get<1>(all);
                auto& magics = std::get<2>(all);
                ClientID id = 0xffffffff;
                uint64_t recv_magic;
                bmp::deserialize(recv_magic, packet.get_readable_data());
                if (magics->contains(recv_magic)) {
                    id = magics->at(recv_magic);
                    magics->erase(recv_magic);
                } else {
                    beammp_debugf("Invalid magic received on UDP from [{}]:{}, ignoring.", ep.address().to_string(), ep.port());
                    continue;
                }
                if (clients->contains(id)) {
                    auto client = clients->at(id);
                    // check if endpoint already exists for this client!
                    auto iter = std::find_if(endpoints->begin(), endpoints->end(), [&](const auto& item) {
                        return item.second == id;
                    });
                    if (iter != endpoints->end()) {
                        // already exists, malicious attempt!
                        beammp_debugf("[{}]:{} tried to replace {}'s UDP endpoint, ignoring.", ep.address().to_string(), ep.port(), id);
                        continue;
                    }
                    // not yet set! nice! set!
                    endpoints->emplace(ep, id);
                    // now transfer them to the next state
                    beammp_debugf("Client {} successfully connected via UDP", client->id);
                    bmp::Packet state_change {
                        .purpose = bmp::Purpose::StateChangeModDownload,
                    };
                    client->tcp_write(state_change);
                    client->state = bmp::State::ModDownload;
                    continue;
                } else {
                    beammp_warnf("Received magic for client who doesn't exist anymore: {}. Ignoring.", id);
                }
            } else {
                handle_packet(m_udp_endpoints->at(ep), packet);
            }
        } catch (const std::exception& e) {
            beammp_errorf("Failed to UDP read: {}", e.what());
        }
    }
}

void Network::disconnect(ClientID id, const std::string& msg) {
    beammp_infof("Disconnecting client {}: {}", id, msg);
    // deadlock-free algorithm to acquire a lock on all these
    // this is a little ugly but saves a headache here in the future
    auto all = boost::synchronize(m_clients, m_udp_endpoints, m_client_magics);
    auto& clients = std::get<0>(all);
    auto& endpoints = std::get<1>(all);
    auto& magics = std::get<2>(all);

    if (clients->contains(id)) {
        auto client = clients->at(id);
        beammp_debugf("Removing client udp magic {}", client->udp_magic);
        magics->erase(client->udp_magic);
    }
    std::erase_if(*endpoints, [&](const auto& item) {
        const auto& [key, value] = item;
        return value == id;
    });
    clients->erase(id);
}
void Network::handle_packet(ClientID id, const bmp::Packet& packet) {
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
        handle_identification(id, packet, client);
        break;
    case bmp::State::Authentication:
        handle_authentication(id, packet, client);
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
void Network::handle_identification(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client) {
    switch (packet.purpose) {
    case bmp::ProtocolVersion: {
        struct bmp::ProtocolVersion protocol_version { };
        protocol_version.deserialize_from(packet.get_readable_data());
        if (protocol_version.version.major != 1) {
            beammp_debugf("{}: Protocol version bad", id);
            // version bad
            bmp::Packet protocol_v_bad_packet {
                .purpose = bmp::ProtocolVersionBad,
            };
            client->tcp_write(protocol_v_bad_packet);
            disconnect(id, fmt::format("bad protocol version: {}.{}.{}", protocol_version.version.major, protocol_version.version.minor, protocol_version.version.patch));
        } else {
            beammp_debugf("{}: Protocol version ok", id);
            // version ok
            bmp::Packet protocol_v_ok_packet {
                .purpose = bmp::ProtocolVersionOk,
            };
            client->tcp_write(protocol_v_ok_packet);
        }
        break;
    }
    case bmp::ClientInfo: {
        struct bmp::ClientInfo cinfo { };
        cinfo.deserialize_from(packet.get_readable_data());
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
        // respond with server info
        auto version = Application::ServerVersion();
        struct bmp::ServerInfo sinfo {
            .program_version = {
                .major = version.major,
                .minor = version.minor,
                .patch = version.patch,
            },
            .implementation = {
                .value = "Official BeamMP Server (BeamMP Ltd.)",
            },
        };
        bmp::Packet sinfo_packet {
            .purpose = bmp::ServerInfo,
            .raw_data = std::vector<uint8_t>(1024),
        };
        sinfo.serialize_to(sinfo_packet.raw_data);
        client->tcp_write(sinfo_packet);
        // now transfer to next state
        bmp::Packet auth_state {
            .purpose = bmp::StateChangeAuthentication,
        };
        client->tcp_write(auth_state);
        client->state = bmp::State::Authentication;
        break;
    }
    default:
        beammp_errorf("Got 0x{:x} in state {}. This is not allowed. Disconnecting the client", uint16_t(packet.purpose), int(client->state));
        disconnect(id, "invalid purpose in current state");
    }
}

void Network::authenticate_user(const std::string& public_key, std::shared_ptr<Client>& client) {
    nlohmann::json AuthReq {};
    std::string auth_res_str {};
    try {
        AuthReq = nlohmann::json {
            { "key", public_key }
        };

        auto Target = "/pkToUser";
        unsigned int ResponseCode = 0;
        auth_res_str = Http::POST(Application::GetBackendUrlForAuth(), 443, Target, AuthReq.dump(), "application/json", &ResponseCode);
    } catch (const std::exception& e) {
        beammp_debugf("Invalid key sent by client {}: {}", client->id, e.what());
        throw std::runtime_error("Public key was of an invalid format");
    }

    try {
        nlohmann::json auth_response = nlohmann::json::parse(auth_res_str);

        if (auth_response["username"].is_string() && auth_response["roles"].is_string()
            && auth_response["guest"].is_boolean() && auth_response["identifiers"].is_array()) {

            *client->name = auth_response["username"];
            *client->role = auth_response["roles"];
            *client->is_guest = auth_response["guest"];
            for (const auto& identifier : auth_response["identifiers"]) {
                auto identifier_str = std::string(identifier);
                auto identifier_sep_idx = identifier_str.find(':');
                client->identifiers->emplace(identifier_str.substr(0, identifier_sep_idx), identifier_str.substr(identifier_sep_idx + 1));
            }
        } else {
            beammp_errorf("Invalid authentication data received from authentication backend for client {}", client->id);
            throw std::runtime_error("Backend failed to authenticate the client");
        }
    } catch (const std::exception& e) {
        beammp_errorf("Client {} sent invalid key. Error was: {}", client->id, e.what());
        throw std::runtime_error("Invalid public key");
    }
}

void Network::handle_authentication(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client) {
    switch (packet.purpose) {
    case bmp::Purpose::PlayerPublicKey: {
        auto packet_data = packet.get_readable_data();
        auto public_key = std::string(packet_data.begin(), packet_data.end());
        try {
            authenticate_user(public_key, client);
        } catch (const std::exception& e) {
            // propragate to client and disconnect
            auto err = std::string(e.what());
            beammp_errorf("Client {} failed to authenticate: {}", id, err);
            bmp::Packet auth_fail_packet {
                .purpose = bmp::Purpose::AuthFailed,
                .raw_data = std::vector<uint8_t>(err.begin(), err.end()),
            };
            client->tcp_write(auth_fail_packet);
            disconnect(id, err);
            return;
        }
        auto Futures = LuaAPI::MP::Engine->TriggerEvent("onPlayerAuth", "", client->name.get(), client->role.get(), client->is_guest.get(), client->identifiers.get());
        TLuaEngine::WaitForAll(Futures);
        bool NotAllowed = std::any_of(Futures.begin(), Futures.end(),
            [](const std::shared_ptr<TLuaResult>& Result) {
                return !Result->Error && Result->Result.is<int>() && bool(Result->Result.as<int>());
            });
        std::string Reason;
        bool NotAllowedWithReason = std::any_of(Futures.begin(), Futures.end(),
            [&Reason](const std::shared_ptr<TLuaResult>& Result) -> bool {
                if (!Result->Error && Result->Result.is<std::string>()) {
                    Reason = Result->Result.as<std::string>();
                    return true;
                }
                return false;
            });

        if (NotAllowed) {
            bmp::Packet auth_fail_packet {
                .purpose = bmp::Purpose::PlayerRejected
            };
            client->tcp_write(auth_fail_packet);
            disconnect(id, "Rejected by a plugin");
            return;
        } else if (NotAllowedWithReason) {
            bmp::Packet auth_fail_packet {
                .purpose = bmp::Purpose::PlayerRejected,
                .raw_data = std::vector<uint8_t>(Reason.begin(), Reason.end()),
            };
            client->tcp_write(auth_fail_packet);
            disconnect(id, fmt::format("Rejected by a plugin for reason: {}", Reason));
            return;
        }
        beammp_debugf("Client {} successfully authenticated as {} '{}'", id, client->role.get(), client->name.get());
        // send auth ok since auth succeeded
        bmp::Packet auth_ok {
            .purpose = bmp::Purpose::AuthOk,
            .raw_data = std::vector<uint8_t>(4),
        };
        // with the player id
        bmp::serialize(client->id, auth_ok.raw_data);
        client->tcp_write(auth_ok);

        // save the udp magic
        m_client_magics->emplace(client->udp_magic, client->id);

        // send the udp start packet, which should get the client to start udp with
        // this packet as the first message
        bmp::Packet udp_start {
            .purpose = bmp::Purpose::StartUDP,
            .raw_data = std::vector<uint8_t>(8),
        };
        bmp::serialize(client->udp_magic, udp_start.raw_data);
        client->tcp_write(udp_start);
        // player must start udp to advance now, so no state change
        break;
    }
    default:
        beammp_errorf("Got 0x{:x} in state {}. This is not allowed. Disconnecting the client", uint16_t(packet.purpose), int(client->state));
        disconnect(id, "invalid purpose in current state");
    }
}
