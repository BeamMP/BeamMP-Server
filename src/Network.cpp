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
#include "Transport.h"
#include "Util.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/system/is_error_condition_enum.hpp>
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

// ======================= Boost helpers =========================

/// Boost::asio + strands + timer magic to make writes timeout after some time.
template <typename HandlerFn>
static void async_write_timeout(Client::Ptr client, const_buffer&& sequence, boost::posix_time::milliseconds timeout_ms, HandlerFn&& handler) {
    struct TimeoutHelper : std::enable_shared_from_this<TimeoutHelper> {
        /// Given a socket (stream), buffer and a completion handler, constructs a state machine.
        TimeoutHelper(Client::Ptr client, ip::tcp::socket& stream, const_buffer buffer, HandlerFn handler)
            : m_client(client)
            , m_stream(stream)
            , m_buffer(std::move(buffer))
            , m_handler_fn(std::move(handler)) { }
        /// Kicks off the timer and async_write, which race to cancel each other.
        /// Whichever completes first gets to cancel the other one.
        /// Effectively, the timer will finish before the write if the write is "timing out",
        /// and if the write finishes beforehand the timeout resets.
        void start(boost::posix_time::milliseconds timeout_ms) {
            // setup the timer to expire in millis ms
            m_timer.expires_from_now(timeout_ms);
            // start waiting on the timer to expire. we give it ourselves (shared ptr via shared_from_this
            // as a copy so that it can call the timeout handler on this object.
            // the whole thing is wrapped in a strand to avoid this happening on two separate thwrites at the same time,
            // i.e. the timer and write finish at the same time on separate thwrites, or other goofy stuff.
            m_timer.async_wait(bind_executor(m_strand, [self = this->shared_from_this()](auto&& ec) {
                self->handle_timeout(ec);
            }));
            // start the write on the same strand, again giving a copy of a shared_ptr to ourselves so the handler can be
            // called.
            boost::asio::async_write(m_stream, m_buffer, bind_executor(m_strand, [self = this->shared_from_this()](auto&& ec, auto size) {
                self->handle_write(ec, size);
            }));
        }

        /// Called when the timer times out.
        void handle_timeout(boost::system::error_code const& ec) {
            // not an error and write() hasn't finished means we need to cancel the stream's write (this can be done
            // by just cancelling the stream, i guess).
            if (not ec and not m_completed) {
                // black-hole the error, because we don't care
                boost::system::error_code sink;
                m_stream.cancel(sink);
            }
        }

        /// Called when the write finishes or errors. An error is considered a completion, and will
        /// also cancel the timer. We don't really care why it completed, we just like that it did.
        void handle_write(boost::system::error_code const& ec, std::size_t size) {
            // this would be weird!
            assert(not m_completed);
            // blackhole the error of the timer cancel operation, we dont care
            boost::system::error_code sink;
            m_timer.cancel(sink);
            // we're done
            m_completed = true;
            // call the original completion handler
            m_handler_fn(ec, size);
        }

        Client::Ptr m_client;
        ip::tcp::socket& m_stream;
        const_buffer m_buffer;
        HandlerFn m_handler_fn;
        boost::asio::strand<ip::tcp::socket::executor_type> m_strand { m_stream.get_executor() };
        boost::asio::deadline_timer m_timer { m_stream.get_executor() };
        bool m_completed = false;
    };

    auto helper = std::make_shared<TimeoutHelper>(client, client->tcp_socket(),
        std::forward<const_buffer>(sequence),
        std::forward<HandlerFn>(handler));
    helper->start(timeout_ms);
}

/// Boost::asio + strands + timer magic to make reads timeout after some time.
template <typename HandlerFn>
static void async_read_timeout(Client::Ptr client, mutable_buffer&& sequence, boost::posix_time::milliseconds timeout_ms, HandlerFn&& handler) {
    struct TimeoutHelper : std::enable_shared_from_this<TimeoutHelper> {
        /// Given a socket (stream), buffer and a completion handler, constructs a state machine.
        TimeoutHelper(Client::Ptr client, ip::tcp::socket& stream, mutable_buffer buffer, HandlerFn handler)
            : m_client(client)
            , m_stream(stream)
            , m_buffer(std::move(buffer))
            , m_handler_fn(std::move(handler)) {
        }
        /// Kicks off the timer and async_read, which race to cancel each other.
        /// Whichever completes first gets to cancel the other one.
        /// Effectively, the timer will finish before the read if the read is "timing out",
        /// and if the read finishes beforehand the timeout resets.
        void start(boost::posix_time::milliseconds timeout_ms) {
            // setup the timer to expire in millis ms
            m_timer.expires_from_now(timeout_ms);
            // start waiting on the timer to expire. we give it ourselves (shared ptr via shared_from_this
            // as a copy so that it can call the timeout handler on this object.
            // the whole thing is wrapped in a strand to avoid this happening on two separate threads at the same time,
            // i.e. the timer and read finish at the same time on separate threads, or other goofy stuff.
            m_timer.async_wait(bind_executor(m_strand, [self = this->shared_from_this()](auto&& ec) {
                self->handle_timeout(ec);
            }));
            // start the read on the same strand, again giving a copy of a shared_ptr to ourselves so the handler can be
            // called.
            boost::asio::async_read(m_stream, m_buffer, bind_executor(m_strand, [self = this->shared_from_this()](auto&& ec, auto size) {
                self->handle_read(ec, size);
            }));
        }

        /// Called when the timer times out.
        void handle_timeout(boost::system::error_code const& ec) {
            // not an error and read() hasn't finished means we need to cancel the stream's read (this can be done
            // by just cancelling the stream, i guess).
            if (not ec and not m_completed) {
                // black-hole the error, because we don't care
                boost::system::error_code sink;
                m_stream.cancel(sink);
            }
        }

        /// Called when the read finishes or errors. An error is considered a completion, and will
        /// also cancel the timer. We don't really care why it completed, we just like that it did.
        void handle_read(boost::system::error_code const& ec, std::size_t size) {
            // this would be weird!
            assert(not m_completed);
            // blackhole the error of the timer cancel operation, we dont care
            boost::system::error_code sink;
            m_timer.cancel(sink);
            // we're done
            m_completed = true;
            // call the original completion handler
            m_handler_fn(ec, size);
        }

        Client::Ptr m_client;
        ip::tcp::socket& m_stream;
        mutable_buffer m_buffer;
        HandlerFn m_handler_fn;
        boost::asio::strand<ip::tcp::socket::executor_type> m_strand { m_stream.get_executor() };
        boost::asio::deadline_timer m_timer { m_stream.get_executor() };
        bool m_completed = false;
    };

    auto helper = std::make_shared<TimeoutHelper>(client, client->tcp_socket(),
        std::forward<mutable_buffer>(sequence),
        std::forward<HandlerFn>(handler));
    helper->start(timeout_ms);
}

// ======================= End of boost helpers =========================

#include <doctest/doctest.h>

void Network::send_to(ClientID id, bmp::Packet& packet) {
    m_clients->at(id)->tcp_write(packet);
}

void Client::tcp_write(bmp::Packet& packet) {
    beammp_tracef("Sending {} to {}", int(packet.purpose), id);
    // finalize the packet (compress etc) and produce header
    auto header = packet.finalize();
    // data has to be a shared_ptr, because we pass it to the async write function which completes later,
    // when this is already out of scope
    auto data = std::make_shared<std::vector<uint8_t>>(bmp::Header::SERIALIZED_SIZE + header.size);
    auto offset = header.serialize_to(*data);
    std::copy(packet.raw_data.begin(), packet.raw_data.end(), data->begin() + long(offset));
    // calculate timeout, which must be at least 500ms
    auto timeout = boost::posix_time::milliseconds(std::max(size_t(500), size_t(std::ceil(double(data->size()) * m_write_byte_timeout))));
    beammp_tracef("Packet of size {} B given a timeout of {}ms ({}s)", data->size(), timeout.total_milliseconds(), timeout.seconds());
    // write header and packet data
    async_write_timeout(
        shared_from_this(), buffer(*data), timeout, [data, this](const boost::system::error_code& ec, size_t) {
            if (ec && ec.value() == boost::system::errc::operation_canceled) {
                // write timeout is fatal
                m_network.disconnect(id, "Write timeout");
            }
        });
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
    try {
        m_tcp_socket.shutdown(boost::asio::socket_base::shutdown_receive);
    } catch (...) {
    }
    m_tcp_thread.interrupt();
    beammp_debugf("Client {} shut down", id);
}

Client::Client(ClientID id, Network& network, ip::tcp::socket&& tcp_socket)
    : id(id)
    , udp_magic(id ^ (uint64_t(std::rand()) << 32) ^ uint64_t(this))
    , m_tcp_socket(std::forward<ip::tcp::socket&&>(tcp_socket))
    , m_network(network)
    , m_tcp_strand(std::make_shared<StrandPtr::element_type>(m_tcp_socket.get_executor())) {
    beammp_debugf("Client {} created", id);
}

bool Client::handle_timeout() {
    if (m_timed_out) {
        m_network.disconnect(id, "Timed out and failed to respond to ping");
        return false;
    } else {
        m_timed_out = true;
        beammp_debugf("Sending ping to {} to confirm timeout", id);
        bmp::Packet packet {
            .purpose = bmp::Purpose::Ping,
        };
        tcp_write(packet);
        return true;
    }
}

void Client::start_tcp() {
    beammp_tracef("{}", __func__);
    m_header.resize(bmp::Header::SERIALIZED_SIZE);
    beammp_tracef("Header buffer size: {}", m_header.size());
    async_read_timeout(shared_from_this(), buffer(m_header), m_read_timeout, [this](const boost::system::error_code& ec, size_t) {
        if (ec && ec.value() == boost::system::errc::operation_canceled) {
            beammp_warnf("Client {} possibly timing out", id);
            if (handle_timeout()) {
                start_tcp();
            }
        } else if (ec) {
            beammp_errorf("TCP read() failed: {}", ec.message());
            m_network.disconnect(id, "read() failed");
        } else {
            if (m_timed_out) {
                m_timed_out = false;
            }
            try {
                bmp::Header hdr {};
                hdr.deserialize_from(m_header);
                beammp_tracef("Got header with purpose {}, size {} from {}", int(hdr.purpose), hdr.size, id);
                // delete previous packet if any exists
                m_packet = {};
                m_packet.purpose = hdr.purpose;
                m_packet.flags = hdr.flags;
                m_packet.raw_data.resize(hdr.size);
                beammp_tracef("Raw data buffer size: {}", m_packet.raw_data.size());
                async_read_timeout(shared_from_this(), buffer(m_packet.raw_data), m_read_timeout, [this](const boost::system::error_code& ec, size_t bytes) {
                    if (ec && ec.value() == boost::system::errc::operation_canceled) {
                        beammp_warnf("Client {} possibly timing out after sending header", id);
                        if (handle_timeout()) {
                            start_tcp();
                        }
                    } else if (ec) {
                        beammp_errorf("TCP read() failed: {}", ec.message());
                        m_network.disconnect(id, "read() failed");
                    } else {
                        if (m_timed_out) {
                            m_timed_out = false;
                        }
                        beammp_tracef("Got body of size {} from {}", bytes, id);
                        m_network.handle_packet(id, m_packet);
                        // recv another packet!
                        start_tcp();
                    }
                });
            } catch (const std::exception& e) {
                beammp_errorf("Error while processing TCP packet from client {}: {}", id, e.what());
                m_network.disconnect(id, "Failed receive TCP or parse packet");
            }
        }
    });
}

bmp::Packet Network::udp_read(ip::udp::endpoint& out_ep) {
    // maximum we can ever expect from udp
    static thread_local std::vector<uint8_t> s_buffer(std::numeric_limits<uint16_t>::max());
    m_udp_socket.receive_from(buffer(s_buffer), out_ep, {});
    bmp::Packet packet;
    bmp::Header header {};
    auto offset = header.deserialize_from(s_buffer);
    packet.purpose = header.purpose;
    packet.flags = header.flags;
    packet.raw_data.resize(header.size);
    std::copy(s_buffer.begin() + offset, s_buffer.begin() + offset + header.size, packet.raw_data.begin());
    return packet;
}

void Network::udp_write(bmp::Packet& packet, const ip::udp::endpoint& ep) {
    auto header = packet.finalize();
    std::vector<uint8_t> data(header.size + bmp::Header::SERIALIZED_SIZE);
    auto offset = header.serialize_to(data);
    std::copy(packet.raw_data.begin(), packet.raw_data.end(), data.begin() + static_cast<long>(offset));
    m_udp_socket.send_to(buffer(data), ep, {});
}

Network::Network()
    : m_tcp_listen_thread(&Network::tcp_listen_main, this)
    , m_udp_read_thread(&Network::udp_read_main, this) {
    Application::RegisterShutdownHandler([this] {
        *m_shutdown = true;
        m_tcp_listen_thread.interrupt();
        m_udp_read_thread.interrupt();
    });
    Application::SetSubsystemStatus("Network", Application::Status::Good);
}

Network::~Network() {
    Application::SetSubsystemStatus("Network", Application::Status::ShuttingDown);
    *m_shutdown = true;
    if (!m_tcp_listen_thread.try_join_for(boost::chrono::seconds(1))) {
        m_tcp_listen_thread.detach();
        Application::SetSubsystemStatus("TCP", Application::Status::Shutdown);
    }
    if (!m_udp_read_thread.try_join_for(boost::chrono::seconds(1))) {
        m_udp_read_thread.detach();
        Application::SetSubsystemStatus("UDP", Application::Status::Shutdown);
    }
    Application::SetSubsystemStatus("Network", Application::Status::Shutdown);
}

void Network::tcp_listen_main() {
    Application::SetSubsystemStatus("TCP", Application::Status::Starting);
    ip::tcp::endpoint listen_ep(ip::address::from_string("0.0.0.0"), static_cast<uint16_t>(Application::Settings.Port));
    boost::system::error_code ec;
    m_tcp_listener.open(listen_ep.protocol(), ec);
    if (ec) {
        beammp_errorf("Failed to open socket: {}", ec.message());
        return;
    }
    socket_base::linger linger_opt {};
    linger_opt.enabled(false);
    m_tcp_listener.set_option(linger_opt, ec);
    if (ec) {
        beammp_errorf("Failed to set up listening socket to not linger / reuse address. "
                      "This may cause the socket to refuse to bind(). Error: {}",
            ec.message());
    }

    m_tcp_acceptor = { m_threadpool, listen_ep };
    m_tcp_acceptor.listen(socket_base::max_listen_connections, ec);
    if (ec) {
        beammp_errorf("listen() failed, which is needed for the server to operate. "
                      "Shutting down. Error: {}",
            ec.message());
        Application::GracefullyShutdown();
    }
    Application::SetSubsystemStatus("TCP", Application::Status::Good);

    // start an accept. this is async and will call itself repeatedly.
    accept();

    // wait for all tasks to complete
    m_threadpool.join();
    Application::SetSubsystemStatus("TCP", Application::Status::Shutdown);
}

void Network::accept() {
    // first create a socket!
    m_temp_socket = ip::tcp::socket(m_threadpool);
    // then use that client's tcp socket to accept into
    m_tcp_acceptor.async_accept(m_temp_socket, [this](const auto& ec) { handle_accept(ec); });
}

void Network::handle_accept(const boost::system::error_code& ec) {
    if (ec) {
        beammp_errorf("Failed accepting new client: {}", ec.message());
    } else {
        auto new_id = new_client_id();
        beammp_debugf("New connection from {}", m_temp_socket.remote_endpoint().address().to_string(), m_temp_socket.remote_endpoint().port());
        Client::Ptr new_client = Client::make_ptr(new_id, *this, std::move(m_temp_socket));
        m_clients->emplace(new_id, new_client);
        new_client->start_tcp();
    }
    accept();
}

void Network::udp_read_main() {
    Application::SetSubsystemStatus("UDP", Application::Status::Starting);
    m_udp_socket = ip::udp::socket(m_threadpool, ip::udp::endpoint(ip::udp::v4(), Application::Settings.Port));
    Application::SetSubsystemStatus("UDP", Application::Status::Good);
    while (!*m_shutdown) {
        try {
            ip::udp::endpoint ep;
            auto packet = udp_read(ep);
            beammp_tracef("UDP recv: {}", int(packet.purpose));
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
                    client->set_udp_endpoint(ep);
                    // now transfer them to the next state
                    beammp_debugf("Client {} successfully connected via UDP", client->id);
                    // send state change and further stuff asynchronously - we dont really care here, just wanna
                    // get out and do udp again!
                    post(m_threadpool, [client, this] {
                        beammp_debugf("Client {} starting mod download", client->id);
                        bmp::Packet state_change {
                            .purpose = bmp::Purpose::StateChangeModDownload,
                        };
                        client->tcp_write(state_change);
                        client->state = bmp::State::ModDownload;
                        // TODO: Get real mods info from *somewhere!*
                        std::string mods = nlohmann::json::array().dump();
                        bmp::Packet mods_info {
                            .purpose = bmp::Purpose::ModsInfo,
                            .raw_data = std::vector<uint8_t>(mods.begin(), mods.end()),
                        };
                        client->tcp_write(mods_info);
                    });
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
    Application::SetSubsystemStatus("UDP", Application::Status::Shutdown);
}

void Network::disconnect(ClientID id, const std::string& msg) {
    // this has to be scheduled, because the thread which did this cannot!!! do it itself.
    beammp_debugf("Scheduling disconnect for {}", id);
    // grab the client ptr
    auto clients = m_clients.synchronize();
    if (clients->contains(id)) {
        auto client = clients->at(id);
        post(context(), [client, id, msg, this] {
            beammp_infof("Disconnecting client {}: {}", id, msg);
            // deadlock-free algorithm to acquire a lock on all these
            // this is a little ugly but saves a headache here in the future
            auto all = boost::synchronize(m_clients, m_udp_endpoints, m_client_magics);
            auto& clients = std::get<0>(all);
            auto& endpoints = std::get<1>(all);
            auto& magics = std::get<2>(all);

            beammp_debugf("Removing client udp magic {}", client->udp_magic);
            magics->erase(client->udp_magic);

            std::erase_if(*endpoints, [&](const auto& item) {
                const auto& [key, value] = item;
                return value == id;
            });
            // TODO: Despawn vehicles owned by this player
            clients->erase(id);
            try {
                client->tcp_socket().shutdown(boost::asio::socket_base::shutdown_both);
                client->tcp_socket().close();
            } catch (...) { }
        });
    } else {
        beammp_debugf("Client {} already disconnected", id);
    }
}

std::unordered_map<ClientID, Client::Ptr> Network::playing_clients() const {
    std::unordered_map<ClientID, Client::Ptr> copy {};
    auto clients = m_clients.synchronize();
    copy.reserve(clients->size());
    for (const auto& [id, client] : *clients) {
        if (client->state == bmp::State::Playing) {
            copy[id] = client;
        }
    }
    return copy;
}
std::unordered_map<ClientID, Client::Ptr> Network::authenticated_clients() const {
    std::unordered_map<ClientID, Client::Ptr> copy {};
    auto clients = m_clients.synchronize();
    copy.reserve(clients->size());
    for (const auto& [id, client] : *clients) {
        if (client->state >= bmp::State::Authentication) {
            copy[id] = client;
        }
    }
    return copy;
}
std::unordered_map<ClientID, Client::Ptr> Network::all_clients() const {
    return *m_clients;
}
size_t Network::authenticated_client_count() const {
    auto clients = m_clients.synchronize();
    return size_t(std::count_if(clients->begin(), clients->end(), [](const auto& pair) {
        return pair.second->state >= bmp::State::Authentication;
    }));
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
    beammp_tracef("Client {} is in state {}", int(id), int(client->state.get()));
    // handle ping immediately if the player is authed
    if (client->state.get() > bmp::State::Authentication && packet.purpose == bmp::Purpose::Ping) {
        beammp_tracef("Got pong from {}", int(id));
        return;
    }
    switch (*client->state) {
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
        handle_mod_download(id, packet, client);
        break;
    case bmp::State::SessionSetup:
        handle_session_setup(id, packet, client);
        break;
    case bmp::State::Playing:
        handle_playing(id, packet, client);
        break;
    case bmp::State::Leaving:
        break;
    }
}

void Network::handle_playing(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client) {
    auto decompressed = packet.get_readable_data();
    uint16_t vid;
    auto offset = bmp::deserialize(vid, decompressed);
    // use this to operate on ;)
    std::span<uint8_t> data(decompressed.begin() + long(offset), decompressed.end());

    // kick?? if vehicle is not owned by this player
    // TODO: What's the best course of action here?
    if (vid != 0xffff && m_vehicles->at(vid)->owner != id) {
        disconnect(id, fmt::format("Tried to change vehicle {}, but owner is {}", vid, id));
        return;
    }

    switch (packet.purpose) {
    case bmp::Purpose::VehicleSpawn: {
        // TODO: Check vehicle spawn limit
        auto vehicles = m_vehicles.synchronize();
        // overwrite the vid since it's definitely invalid anyways
        vid = new_vehicle_id();
        auto vehicle = std::make_shared<Vehicle>(data);
        vehicles->emplace(vid, vehicle);
        beammp_debugf("Client {} spawned vehicle {}", id, vid);
        break;
    }
    case bmp::Purpose::VehicleDelete: {
        auto vehicles = m_vehicles.synchronize();
        vehicles->erase(vid);
        beammp_debugf("Client {} deleted vehicle {}", id, vid);
        break;
    }
    case bmp::Purpose::VehicleReset: {
        auto vehicles = m_vehicles.synchronize();
        vehicles->at(vid)->reset_status(data);
        beammp_debugf("Client {} reset vehicle {}", id, vid);
        break;
    }
    case bmp::Purpose::VehicleEdited: {
        auto vehicles = m_vehicles.synchronize();
        auto veh_data = vehicles->at(vid)->data.synchronize();
        veh_data->resize(data.size());
        std::copy(data.begin(), data.end(), veh_data->begin());
        beammp_debugf("Client {} edited vehicle {}", id, vid);
        break;
    }
    case bmp::Purpose::VehicleCouplerChanged: {
        break;
    }
    case bmp::Purpose::SpectatorSwitched: {
        break;
    }
    case bmp::Purpose::ApplyInput: {
        break;
    }
    case bmp::Purpose::ApplyElectrics: {
        break;
    }
    case bmp::Purpose::ApplyNodes: {
        break;
    }
    case bmp::Purpose::ApplyBreakgroups: {
        break;
    }
    case bmp::Purpose::ApplyPowertrain: {
        break;
    }
    case bmp::Purpose::ApplyPosition: {
        auto vehicles = m_vehicles.synchronize();
        vehicles->at(vid)->update_status(data);
        break;
    }
    case bmp::Purpose::ChatMessage: {
        std::string msg(data.begin(), data.end());
        LogChatMessage(client->name.get(), int(client->id), msg);
        break;
    }
    case bmp::Purpose::Event: {
        break;
    }
    case bmp::Purpose::StateChangeLeaving: {
        beammp_infof("Client {} leaving", id);
        // TODO: Not implemented properly, change to state and leave cleanly instead.
        disconnect(id, "Leaving");
        break;
    }
    default:
        beammp_errorf("Got 0x{:x} in state {}. This is not allowed. Disconnecting the client", uint16_t(packet.purpose), int(client->state.get()));
        disconnect(id, "invalid purpose in current state");
        break;
    }
    auto clients = playing_clients();
    bmp::Packet broadcast {
        .purpose = packet.purpose,
        .flags = packet.flags,
        .raw_data = {},
    };
    // 4 extra bytes for the pid header
    broadcast.raw_data.resize(packet.raw_data.size() + 4);
    // write pid
    offset = bmp::serialize(uint32_t(id), broadcast.raw_data);
    // write all raw data from the original - this is ok because we also copied the flags,
    // so if this is compressed, we send it as compressed and don't try to compress it again.
    std::copy(packet.raw_data.begin(), packet.raw_data.end(), broadcast.raw_data.begin() + long(offset));
    // add playing header

    // broadcast packets to clients based on their category
    switch (bmp::category_of(packet.purpose)) {
    case bmp::Category::Position:
        for (auto& [this_id, this_client] : clients) {
            udp_write(broadcast, this_client->udp_endpoint());
        }
        break;
    case bmp::Category::None:
        beammp_warnf("Category 'None' for packet purpose {} in state {} is unexpected", int(packet.purpose), int(client->state.get()));
        break;
    case bmp::Category::Nodes:
    case bmp::Category::Vehicle:
    case bmp::Category::Input:
    case bmp::Category::Electrics:
    case bmp::Category::Powertrain:
    case bmp::Category::Chat:
    case bmp::Category::Event:
    default:
        for (auto& [this_id, this_client] : clients) {
            this_client->tcp_write(broadcast);
        }
        break;
    }
}

void Network::handle_session_setup(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client) {
    switch (packet.purpose) {
    case bmp::Purpose::SessionReady: {
        beammp_infof("Client {} is synced", id);
        bmp::Packet state_change {
            .purpose = bmp::Purpose::StateChangePlaying,
        };
        client->tcp_write(state_change);
        break;
    }
    default:
        beammp_errorf("Got 0x{:x} in state {}. This is not allowed. Disconnecting the client", uint16_t(packet.purpose), int(client->state.get()));
        disconnect(id, "invalid purpose in current state");
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
                .value = "Official BeamMP Server",
            },
        };
        bmp::Packet sinfo_packet {
            .purpose = bmp::ServerInfo,
            .raw_data = std::vector<uint8_t>(1024),
        };
        auto offset = sinfo.serialize_to(sinfo_packet.raw_data);
        sinfo_packet.raw_data.resize(offset);
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
        beammp_errorf("Got 0x{:x} in state {}. This is not allowed. Disconnecting the client", uint16_t(packet.purpose), int(client->state.get()));
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

void Network::handle_mod_download(ClientID id, const bmp::Packet& packet, std::shared_ptr<Client>& client) {
    switch (packet.purpose) {
    case bmp::Purpose::ModsSyncDone: {
        beammp_debugf("Client {} is done with mods sync", id);
        beammp_debugf("Sending map info to client {}", id);
        bmp::Packet map_info {
            .purpose = bmp::Purpose::MapInfo,
            .raw_data = std::vector<uint8_t>(Application::Settings.MapName.begin(), Application::Settings.MapName.end()),
        };
        client->tcp_write(map_info);
        beammp_debugf("Client {} entering session setup", id);
        bmp::Packet state_change {
            .purpose = bmp::Purpose::StateChangeSessionSetup,
        };
        *client->state = bmp::State::SessionSetup;
        client->tcp_write(state_change);
        beammp_infof("Client {} starting session sync.", id);
        beammp_debugf("Syncing {} client(s) and {} vehicle(s) to client {}", m_clients->size(), m_vehicles->size(), id);
        // immediately start with the player+vehicle info
        bmp::Packet players_info {
            .purpose = bmp::Purpose::PlayersVehiclesInfo,
            .raw_data = {},
        };
        try {
            auto players_info_json = build_players_info();
            beammp_tracef("Players and vehicles info: {}", players_info_json.dump(4));
            auto serialized = players_info_json.dump();
            players_info.raw_data = std::vector<uint8_t>(serialized.begin(), serialized.end());
        } catch (const std::exception& e) {
            beammp_errorf("Failed to construct players and vehicles info for session setup: {}. This is not recoverable, kicking client.", e.what());
            disconnect(id, "Internal server error: Session setup failed to construct players and vehicles info object");
            return;
        }
        client->tcp_write(players_info);
        break;
    }
    default:
        beammp_errorf("Got 0x{:x} in state {}. This is not allowed. Disconnecting the client", uint16_t(packet.purpose), int(client->state.get()));
        disconnect(id, "invalid purpose in current state");
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
        beammp_errorf("Got 0x{:x} in state {}. This is not allowed. Disconnecting the client", uint16_t(packet.purpose), int(client->state.get()));
        disconnect(id, "invalid purpose in current state");
    }
}
std::optional<Client::Ptr> Network::get_client(ClientID id, bmp::State min_state) const {
    auto clients = m_clients.synchronize();
    if (clients->contains(id)) {
        auto client = clients->at(id);
        if (client->state >= min_state) {
            return clients->at(id);
        } else {
            beammp_warnf("Tried to get client {}, but client is not yet in state {} (is in state {})", id, int(min_state), int(client->state.get()));
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}
std::unordered_map<VehicleID, Vehicle::Ptr> Network::get_vehicles_owned_by(ClientID id) {
    auto vehicles = m_vehicles.synchronize();
    std::unordered_map<VehicleID, Vehicle::Ptr> result {};
    for (const auto& [vid, vehicle] : *vehicles) {
        if (vehicle->owner == id) {
            result[vid] = vehicle;
        }
    }
    return result;
}

void Vehicle::refresh_cache(std::unique_lock<std::recursive_mutex>& lock) {
    (void)lock;
    if (!m_needs_refresh) {
        return;
    }
    try {
        auto json = nlohmann::json::parse(m_status_data.data());
        if (json["rvel"].is_array()) {
            auto array = json["rvel"].get<std::vector<float>>();
            m_rvel = {
                array.at(0),
                array.at(1),
                array.at(2)
            };
        }

        if (json["rot"].is_array()) {
            auto array = json["rot"].get<std::vector<float>>();
            m_rot = {
                array.at(0),
                array.at(1),
                array.at(2),
                array.at(3),
            };
        }
        if (json["vel"].is_array()) {
            auto array = json["vel"].get<std::vector<float>>();
            m_vel = {
                array.at(0),
                array.at(1),
                array.at(2)
            };
        }
        if (json["pos"].is_array()) {
            auto array = json["pos"].get<std::vector<float>>();
            m_pos = {
                array.at(0),
                array.at(1),
                array.at(2)
            };
        }
        if (json["tim"].is_number()) {
            m_time = json["tim"].get<float>();
        }
    } catch (const std::exception& e) {
        beammp_errorf("Invalid position data: {}", e.what());
    }
    m_needs_refresh = false;
}

TEST_CASE("Vehicle position parse, cache, access") {
    Vehicle veh { {} };
    std::string str = R"({"rvel":[0.034001241344458,0.016966195008928,-0.0032029844877877],"rot":[-0.0012675799979579,0.0014056711767528,0.94126306518056,0.3376688606555],"tim":66.978502945043,"vel":[-18.80228647297,22.830758602197,0.0011466381380035],"pos":[562.68027268429,-379.27891669179,160.40605946989],"ping":0.032000000871718})";
    veh.update_status(std::vector<uint8_t>(str.begin(), str.end()));
    auto status = veh.get_status();
    constexpr double EPS = 0.00001;
    CHECK_LT(std::abs(status.rvel.x - 0.034001241344458f), EPS);
    CHECK_LT(std::abs(status.rvel.y - 0.016966195008928f), EPS);
    CHECK_LT(std::abs(status.rvel.z - -0.0032029844877877f), EPS);
    CHECK_LT(std::abs(status.rot.x - -0.0012675799979579f), EPS);
    CHECK_LT(std::abs(status.rot.y - 0.0014056711767528f), EPS);
    CHECK_LT(std::abs(status.rot.z - 0.94126306518056f), EPS);
    CHECK_LT(std::abs(status.rot.w - 0.3376688606555f), EPS);
    CHECK_LT(std::abs(status.time - 66.978502945043f), EPS);
    CHECK_LT(std::abs(status.vel.x - -18.80228647297f), EPS);
    CHECK_LT(std::abs(status.vel.y - 22.830758602197f), EPS);
    CHECK_LT(std::abs(status.vel.z - 0.0011466381380035f), EPS);
    CHECK_LT(std::abs(status.pos.x - 562.68027268429f), EPS);
    CHECK_LT(std::abs(status.pos.y - -379.27891669179f), EPS);
    CHECK_LT(std::abs(status.pos.z - 160.40605946989f), EPS);
}
size_t Network::guest_count() const {
    auto clients = m_clients.synchronize();
    return size_t(std::count_if(clients->begin(), clients->end(), [](const auto& pair) { return pair.second->is_guest; }));
}

size_t Network::clients_in_state_count(bmp::State state) const {
    auto clients = m_clients.synchronize();
    return size_t(std::count_if(clients->begin(), clients->end(), [&state](const auto& pair) { return pair.second->state == state; }));
}

size_t Network::vehicle_count() const { return m_vehicles->size(); }
nlohmann::json Network::build_players_info() {
    auto all = boost::synchronize(m_clients, m_vehicles);
    auto& clients = std::get<0>(all);
    auto& vehicles = std::get<1>(all);
    nlohmann::json info = nlohmann::json::array();
    for (const auto& [id, client] : *clients) {
        auto obj = nlohmann::json({
            { "name", client->name.get() },
            { "id", client->id },
            { "role", client->role },
        });
        obj["vehicles"] = nlohmann::json::array();
        // get all vehicles owned by this client
        // cant use the helper function since that locks as well, that's not clean
        for (const auto& [vid, vehicle] : *vehicles) {
            if (vehicle->owner == id) {
                // vehicle owned by client
                auto data = vehicle->data.synchronize();
                auto status = vehicle->get_raw_status();
                obj["vehicles"] = nlohmann::json::object({
                    { "id", vid },
                    { "data", nlohmann::json::parse(data->begin(), data->end()) },
                    { "status", nlohmann::json::parse(status.begin(), status.end()) },
                });
            }
        }
        info.push_back(std::move(obj));
    }
    return info;
}

std::optional<Vehicle::Ptr> Network::get_vehicle(VehicleID id) {
    auto vehicles = m_vehicles.synchronize();
    if (vehicles->contains(id)) {
        return vehicles->at(id);
    } else {
        return std::nullopt;
    }
}

ClientID Network::new_client_id() {
    static Sync<ClientID> s_id { 0 };
    auto id = s_id.synchronize();
    ClientID new_id = *id;
    *id += 1;
    return new_id;
}

VehicleID Network::new_vehicle_id() {
    static Sync<VehicleID> s_id { 0 };
    auto id = s_id.synchronize();
    VehicleID new_id = *id;
    *id += 1;
    return new_id;
}

void Network::send_system_chat_message(const std::string& msg, ClientID to) {
    auto packet = make_playing_packet(bmp::Purpose::ChatMessage, 0xffffffff, 0xffff, std::vector<uint8_t>(msg.begin(), msg.end()));
    auto clients = playing_clients();
    if (to != 0xffffffff) {
        for (auto& [this_id, this_client] : clients) {
            this_client->tcp_write(packet);
        }
        LogChatMessage("<System>", -1, msg);
    } else {
        for (auto& [this_id, this_client] : clients) {
            if (this_id == to) {
                this_client->tcp_write(packet);
                break;
            }
        }
        LogChatMessage(fmt::format("<System to {}>", to), -1, msg);
    }
}

bmp::Packet Network::make_playing_packet(bmp::Purpose purpose, ClientID from_id, VehicleID veh_id, const std::vector<uint8_t>& data) {
    bmp::Packet packet {
        .purpose = purpose,
        .raw_data = {},
    };
    packet.raw_data.resize(data.size() + 6);
    auto offset = bmp::serialize(uint32_t(from_id), packet.raw_data);
    offset += bmp::serialize(uint16_t(veh_id), std::span<uint8_t>(packet.raw_data.begin() + long(offset), packet.raw_data.end()));
    std::copy(data.begin(), data.end(), packet.raw_data.begin() + long(offset));
    return packet;
}
void Vehicle::reset_status(std::span<const uint8_t> status_data) {
    auto json = nlohmann::json::parse(status_data.data());
    if (json["rvel"].is_array()) {
        auto array = json["rvel"].get<std::vector<float>>();
        m_rvel = {
            array.at(0),
            array.at(1),
            array.at(2)
        };
    } else {
        m_rvel = {};
    }

    if (json["rot"].is_array()) {
        auto array = json["rot"].get<std::vector<float>>();
        m_rot = {
            array.at(0),
            array.at(1),
            array.at(2),
            array.at(3),
        };
    } else {
        m_rot = {};
    }

    if (json["vel"].is_array()) {
        auto array = json["vel"].get<std::vector<float>>();
        m_vel = {
            array.at(0),
            array.at(1),
            array.at(2)
        };
    } else {
        m_vel = {};
    }

    if (json["pos"].is_array()) {
        auto array = json["pos"].get<std::vector<float>>();
        m_pos = {
            array.at(0),
            array.at(1),
            array.at(2)
        };
    } else {
        m_pos = {};
    }

    if (json["tim"].is_number()) {
        m_time = json["tim"].get<float>();
    } else {
        m_time = {};
    }
}

Vehicle::Status Vehicle::get_status() {
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

void Vehicle::update_status(std::span<const uint8_t> raw_packet) {
    std::unique_lock lock(m_mtx);
    m_needs_refresh = true;
    m_status_data.resize(raw_packet.size());
    std::copy(raw_packet.begin(), raw_packet.end(), m_status_data.begin());
}
Client::Ptr Client::make_ptr(ClientID new_id, class Network& network, ip::tcp::socket&& tcp_socket) {
    return Client::Ptr(new Client(new_id, network, std::forward<ip::tcp::socket&&>(tcp_socket)));
}
