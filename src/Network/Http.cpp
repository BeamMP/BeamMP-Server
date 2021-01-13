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
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>

// UNUSED?!
std::string HttpRequest(const std::string& host, int port, const std::string& target) {
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

std::string PostHTTP(const std::string& host, int port, const std::string& target, const std::string& Fields, bool json) {
    try {
        net::io_context io;
        tcp::resolver resolver(io);
        beast::tcp_stream stream(io);
        auto const results = resolver.resolve(host, std::to_string(port));
        stream.connect(results);

        http::request<http::string_body> req { http::verb::post, target, 11 /* http 1.1 */ };

        req.set(http::field::host, host);
        if (json) {
            req.set(http::field::content_type, "application/json");
        }
        req.set(http::field::content_length, boost::lexical_cast<std::string>(Fields.size()));
        req.set(http::field::body, Fields.c_str());
        // tell the server what we are (boost beast)
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.prepare_payload();

        stream.expires_after(std::chrono::seconds(5));

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
