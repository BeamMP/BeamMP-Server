#include "Network.h"
#include "Common.h"

Packet Client::tcp_read(boost::system::error_code& ec) {
    std::unique_lock lock(m_tcp_read_mtx);
    return Packet();
    // CONTINUE
}

void Client::tcp_write(const Packet& packet, boost::system::error_code& ec) {
    std::unique_lock lock(m_tcp_write_mtx);
    auto header = packet.header();
    std::vector<uint8_t> header_data(bmp::Header::SERIALIZED_SIZE);
    if (header.flags != bmp::Flags::None) {
        beammp_errorf("Flags are not implemented");
    }
    header.serialize_to(header_data);
    write(m_tcp_socket, buffer(header_data), ec);
    if (!ec) {
        write(m_tcp_socket, buffer(packet.data), ec);
    }
}

Packet Client::udp_read(boost::system::error_code& ec, ip::udp::socket& socket) {
    // maximum we can ever expect from udp
    static thread_local std::vector<uint8_t> s_buffer(std::numeric_limits<uint16_t>::max());
    std::unique_lock lock(m_udp_read_mtx);
    socket.receive_from(buffer(s_buffer),m_udp_ep, {}, ec);
    if (!ec) {
        Packet packet;
        bmp::Header header{};
        auto offset = header.deserialize_from(s_buffer);
        if (header.flags != bmp::Flags::None) {
            beammp_errorf("Flags are not implemented");
            return {};
        }
    }
    return {};
}

void Client::udp_write(const Packet& packet, ip::udp::socket& socket, boost::system::error_code& ec) {
    auto header = packet.header();
    std::vector<uint8_t> data(header.size + bmp::Header::SERIALIZED_SIZE);
    auto offset = header.serialize_to(data);
    std::copy(packet.data.begin(), packet.data.end(), data.begin() + static_cast<long>(offset));
    socket.send_to(buffer(data), m_udp_ep, {}, ec);
}

Client::~Client() {
    m_tcp_socket.shutdown(boost::asio::socket_base::shutdown_receive);
}

Client::Client(ip::udp::endpoint& ep, ip::tcp::socket&& socket)
    : m_udp_ep(ep)
    , m_tcp_socket(std::forward<ip::tcp::socket&&>(socket)) {
}
void Client::tcp_write_file_raw(const std::filesystem::path& path, boost::system::error_code& ec) {
#if defined(BEAMMP_LINUX)
#elif defined(BEAMMP_WINDOWS)
#else
#endif
}
bmp::Header Packet::header() const {
    return {
        .purpose = purpose,
        .flags = bmp::Flags::None,
        .rsv = 0,
        .size = static_cast<uint32_t>(data.size()),
    };
}