// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 4/9/2020
///

#include "CustomAssert.h"
#include <iostream>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
namespace ssl = net::ssl; // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

// UNUSED?!
std::string HttpRequest(const std::string& host, int port, const std::string& target) {
    // FIXME: this is likely not very well written.
    // if it causes issues, yell at me and I'll fix it asap. - Lion
    try {
        net::io_context io;
        tcp::resolver resolver(io);
        beast::tcp_stream stream(io);
        auto const results = resolver.resolve(host, std::to_string(port));
        stream.connect(results);

        http::request<http::string_body> req { http::verb::get, target, 11 /* http 1.1 */ };

        req.set(http::field::host, host);
        // tell the server what we are (boost beast)
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(stream, req);

        // used for reading
        beast::flat_buffer buffer;
        http::response<http::string_body> response;

        http::read(stream, buffer, response);

        std::string result(response.body());

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != beast::errc::not_connected) {
            throw beast::system_error { ec }; // goes down to `return "-1"` anyways
        }

        return result;

    } catch (const std::exception& e) {
        error(e.what());
        return "-1";
    }
}

std::string PostHTTP(const std::string& host, const std::string& target, const std::unordered_map<std::string, std::string>& fields, const std::string& body, bool json) {
    //try {
    net::io_context io;

    // The SSL context is required, and holds certificates
    ssl::context ctx(ssl::context::tlsv13);

    ctx.set_verify_mode(ssl::verify_none);

    tcp::resolver resolver(io);
    beast::ssl_stream<beast::tcp_stream> stream(io, ctx);
    auto const results = resolver.resolve(tcp::v6(), host, std::to_string(443));
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        boost::system::error_code ec { static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
        // FIXME: we could throw and crash, if we like
        // throw boost::system::system_error { ec };
        debug("POST " + host + target + " failed.");
        return "-1";
    }
    beast::get_lowest_layer(stream).connect(results);
    stream.handshake(ssl::stream_base::client);
    http::request<http::string_body> req { http::verb::post, target, 11 /* http 1.1 */ };

    req.set(http::field::host, host);
    req.set("X-Forwarded-For", HttpRequest("api.ipify.org", 80, "/"));
    if (!body.empty()) {
        if (json) {
            // FIXME: json is untested.
            req.set(http::field::content_type, "application/json");
        } else {
            req.set(http::field::content_type, "application/x-www-form-urlencoded");
        }
        req.set(http::field::content_length, boost::lexical_cast<std::string>(body.size()));
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
    http::response<http::dynamic_body> response;

    http::read(stream, buffer, response);

    std::stringstream result;
    result << response;

    beast::error_code ec;
    stream.shutdown(ec);
    // IGNORING ec

    // info(result.str());
    std::string debug_response_str;
    std::getline(result, debug_response_str);
    debug("POST " + host + target + ": " + debug_response_str);
    return result.str();

    /*} catch (const std::exception& e) {
        error(e.what());
        return "-1";
    }*/
}
