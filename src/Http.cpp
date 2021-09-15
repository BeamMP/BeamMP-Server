#include "Http.h"

#include "Common.h"
#undef beammp_error

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
namespace ssl = net::ssl; // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

std::string Http::GET(const std::string& host, int port, const std::string& target, unsigned int* status) {
    try {
        // Check command line arguments.
        int version = 11;

        // The io_context is required for all I/O
        net::io_context ioc;

        // The SSL context is required, and holds certificates
        ssl::context ctx(ssl::context::tlsv12_client);

        // This holds the root certificate used for verification
        // we don't do / have this
        // load_root_certificates(ctx);

        // Verify the remote server's certificate
        ctx.set_verify_mode(ssl::verify_none);

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            beast::error_code ec { static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
            throw beast::system_error { ec };
        }

        // Look up the domain name
        auto const results = resolver.resolve(host.c_str(), std::to_string(port));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream).connect(results);

        // Perform the SSL handshake
        stream.handshake(ssl::stream_base::client);

        // Set up an HTTP GET request message
        http::request<http::string_body> req { http::verb::get, target, version };
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Gracefully close the stream
        beast::error_code ec;
        stream.shutdown(ec);
        if (ec == net::error::eof) {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec = {};
        }

        if (status) {
            *status = res.base().result_int();
        }

        if (ec)
            throw beast::system_error { ec };

        // If we get here then the connection is closed gracefully
        return std::string(res.body());
    } catch (std::exception const& e) {
        Application::Console().Write(__func__ + std::string(": ") + e.what());
        return ErrorString;
    }
}

std::string Http::POST(const std::string& host, int port, const std::string& target, const std::unordered_map<std::string, std::string>& fields, const std::string& body, const std::string& ContentType, unsigned int* status) {
    try {
        net::io_context io;

        // The SSL context is required, and holds certificates
        ssl::context ctx(ssl::context::tlsv13);

        ctx.set_verify_mode(ssl::verify_none);

        tcp::resolver resolver(io);
        beast::ssl_stream<beast::tcp_stream> stream(io, ctx);
        decltype(resolver)::results_type results;
        auto try_connect_with_protocol = [&](tcp protocol) {
            try {
                results = resolver.resolve(protocol, host, std::to_string(port));
                if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
                    boost::system::error_code ec { static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                    // FIXME: we could throw and crash, if we like
                    // throw boost::system::system_error { ec };
                    //debug("POST " + host + target + " failed.");
                    return false;
                }
                beast::get_lowest_layer(stream).connect(results);
            } catch (const boost::system::system_error&) {
                return false;
            }
            return true;
        };
        //bool ok = try_connect_with_protocol(tcp::v6());
        //if (!ok) {
        //debug("IPv6 connect failed, trying IPv4");
        bool ok = try_connect_with_protocol(tcp::v4());
        if (!ok) {
            //error("failed to resolve or connect in POST " + host + target);
            return "-1";
        }
        //}
        stream.handshake(ssl::stream_base::client);
        http::request<http::string_body> req { http::verb::post, target, 11 /* http 1.1 */ };

        req.set(http::field::host, host);
        if (!body.empty()) {
            req.set(http::field::content_type, ContentType); // "application/json"
            // "application/x-www-form-urlencoded"
            req.set(http::field::content_length, std::to_string(body.size()));
            req.body() = body;
            // info("body is " + body + " (" + req.body() + ")");
            // info("content size is " + std::to_string(body.size()) + " (" + boost::lexical_cast<std::string>(body.size()) + ")");
        }
        for (const auto& pair : fields) {
            // info("setting " + pair.first + " to " + pair.second);
            req.set(pair.first, pair.second);
        }

        std::stringstream oss;
        oss << req;

        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(5));

        http::write(stream, req);

        // used for reading
        beast::flat_buffer buffer;
        http::response<http::string_body> response;

        http::read(stream, buffer, response);

        if (status) {
            *status = response.base().result_int();
        }

        std::stringstream result;
        result << response;

        beast::error_code ec;
        stream.shutdown(ec);
        // IGNORING ec

        // info(result.str());
        std::string debug_response_str;
        std::getline(result, debug_response_str);

        //debug("POST " + host + target + ": " + debug_response_str);
        return std::string(response.body());

    } catch (const std::exception& e) {
        Application::Console().Write(__func__ + std::string(": ") + e.what());
        return ErrorString;
    }
}
