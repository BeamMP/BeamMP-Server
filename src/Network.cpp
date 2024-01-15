#include "Network.h"
#include "Common.h"
#include "Environment.h"

#if defined(BEAMMP_LINUX)
#include <cstring>
#include <cerrno>
#include <sys/sendfile.h>
#include <unistd.h>
#elif defined(BEAMMP_WINDOWS)
#else
#include <boost/iostreams/device/mapped_file.hpp>
#endif

Packet Client::tcp_read() {
    std::unique_lock lock(m_tcp_read_mtx);
    Packet packet{};
    std::vector<uint8_t> header_buffer(bmp::Header::SERIALIZED_SIZE);
    read(m_tcp_socket, buffer(header_buffer));
    bmp::Header hdr{};
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

Packet Client::udp_read(ip::udp::socket& socket) {
    // maximum we can ever expect from udp
    static thread_local std::vector<uint8_t> s_buffer(std::numeric_limits<uint16_t>::max());
    std::unique_lock lock(m_udp_read_mtx);
    socket.receive_from(buffer(s_buffer), m_udp_ep, {});
    Packet packet;
    bmp::Header header {};
    auto offset = header.deserialize_from(s_buffer);
    if (header.flags != bmp::Flags::None) {
        beammp_errorf("Flags are not implemented");
        return {};
    }
    return packet;
}

void Client::udp_write(const Packet& packet, ip::udp::socket& socket) {
    auto header = packet.header();
    std::vector<uint8_t> data(header.size + bmp::Header::SERIALIZED_SIZE);
    auto offset = header.serialize_to(data);
    std::copy(packet.data.begin(), packet.data.end(), data.begin() + static_cast<long>(offset));
    socket.send_to(buffer(data), m_udp_ep, {});
}

Client::~Client() {
    m_tcp_socket.shutdown(boost::asio::socket_base::shutdown_receive);
}

Client::Client(ip::udp::endpoint& ep, ip::tcp::socket&& socket)
    : m_udp_ep(ep)
    , m_tcp_socket(std::forward<ip::tcp::socket&&>(socket)) {
}
void Client::tcp_write_file_raw(const std::filesystem::path& path) {
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
    std::unique_lock lock(m_tcp_write_mtx);
    write(m_tcp_socket, buffer(f.data(), f.size()));
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