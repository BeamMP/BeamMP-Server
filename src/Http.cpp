#include "Http.h"

#include "Common.h"
#undef error

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <map>

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

        // ignore ec

        // If we get here then the connection is closed gracefully
        return std::string(res.body());
    } catch (std::exception const& e) {
        Application::Console().Write(__func__ + std::string(": ") + e.what());
        return "-1";
    }
}

std::string Http::POST(const std::string& host, const std::string& target, const std::unordered_map<std::string, std::string>& fields, const std::string& body, bool json, int* status) {
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
                results = resolver.resolve(protocol, host, std::to_string(443));
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
            Application::Console().Write("[ERROR] failed to resolve or connect in POST " + host + target);
            Sentry.AddErrorBreadcrumb("failed to resolve or connect to " + host + target, __FILE__, std::to_string(__LINE__)); // FIXME: this is ugly.
            return "-1";
        }
        //}
        stream.handshake(ssl::stream_base::client);
        http::request<http::string_body> req { http::verb::post, target, 11 /* http 1.1 */ };

        req.set(http::field::host, host);
        if (!body.empty()) {
            if (json) {
                req.set(http::field::content_type, "application/json");
            } else {
                req.set(http::field::content_type, "application/x-www-form-urlencoded");
            }
            req.set(http::field::content_length, std::to_string(body.size()));
            req.body() = body;
            // info("body is " + body + " (" + req.body() + ")");
            // info("content size is " + std::to_string(body.size()) + " (" + boost::lexical_cast<std::string>(body.size()) + ")");
        }
        for (const auto& pair : fields) {
            // info("setting " + pair.first + " to " + pair.second);
            req.set(pair.first, pair.second);
        }

        std::unordered_map<std::string, std::string> request_data;
        for (const auto& header : req.base()) {
            // need to do explicit casts to convert string_view to string
            // since string_view may not be null-terminated (and in fact isn't, here)
            std::string KeyString(header.name_string());
            std::string ValueString(header.value());
            request_data[KeyString] = ValueString;
        }
        Sentry.SetContext("https-post-request-data", request_data);

        std::stringstream oss;
        oss << req;

        beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(5));

        http::write(stream, req);

        // used for reading
        beast::flat_buffer buffer;
        http::response<http::string_body> response;

        http::read(stream, buffer, response);

        std::unordered_map<std::string, std::string> response_data;
        response_data["reponse-code"] = std::to_string(response.result_int());
        if (status) {
            *status = response.result_int();
        }
        for (const auto& header : response.base()) {
            // need to do explicit casts to convert string_view to string
            // since string_view may not be null-terminated (and in fact isn't, here)
            std::string KeyString(header.name_string());
            std::string ValueString(header.value());
            response_data[KeyString] = ValueString;
        }
        Sentry.SetContext("https-post-response-data", response_data);

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
        Application::Console().Write(e.what());
        Sentry.AddErrorBreadcrumb(e.what(), __FILE__, std::to_string(__LINE__)); // FIXME: this is ugly.
        return "-1";
    }
}

// RFC 2616, RFC 7231
static std::map<size_t, const char*> Map = {
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 102, "Processing" },
    { 103, "Early Hints" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 207, "Multi-Status" },
    { 208, "Already Reported" },
    { 226, "IM Used" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 306, "(Unused)" },
    { 307, "Temporary Redirect" },
    { 308, "Permanent Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Payload Too Large" },
    { 414, "URI Too Long" },
    { 415, "Unsupported Media Type" },
    { 416, "Range Not Satisfiable" },
    { 417, "Expectation Failed" },
    { 421, "Misdirected Request" },
    { 422, "Unprocessable Entity" },
    { 423, "Locked" },
    { 424, "Failed Dependency" },
    { 425, "Too Early" },
    { 426, "Upgrade Required" },
    { 428, "Precondition Required" },
    { 429, "Too Many Requests" },
    { 431, "Request Header Fields Too Large" },
    { 451, "Unavailable For Legal Reasons" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },
    { 506, "Variant Also Negotiates" },
    { 507, "Insufficient Storage" },
    { 508, "Loop Detected" },
    { 510, "Not Extended" },
    { 511, "Network Authentication Required" },
    // cloudflare status codes
    { 520, "(CDN) Web Server Returns An Unknown Error" },
    { 521, "(CDN) Web Server Is Down" },
    { 522, "(CDN) Connection Timed Out" },
    { 523, "(CDN) Origin Is Unreachable" },
    { 524, "(CDN) A Timeout Occurred" },
    { 525, "(CDN) SSL Handshake Failed" },
    { 526, "(CDN) Invalid SSL Certificate" },
    { 527, "(CDN) Railgun Listener To Origin Error" },
    { 530, "(CDN) 1XXX Internal Error" },
};

std::string Http::Status::ToString(int code) {
    if (Map.find(code) != Map.end()) {
        return Map.at(code);
    } else {
        return std::to_string(code);
    }
}
