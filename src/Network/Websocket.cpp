///
/// Created by Anonymous275 on 11/6/2020
///
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "Security/Enc.h"
#include <iostream>
#include "Logger.h"
#include <thread>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
//ws.write(asio::buffer("Hello, world!"));
// asio::connect(sock,r.resolve(asio::ip::tcp::resolver::query{host, "80"}));
// beast::websocket::stream<asio::ip::tcp::socket&> ws(sock);
//    ws.handshake(host,"/");
void SyncData(){
    DebugPrintTID();
    using namespace boost;
    std::string const host = "95.216.35.232";
    net::io_context ioc;

    tcp::resolver r(ioc);

    websocket::stream<tcp::socket> ws{ioc};
    auto const results = r.resolve(host, "3600");
    net::connect(ws.next_layer(), results.begin(), results.end());


}


void WebsocketInit(){
    std::thread t1(SyncData);
    t1.detach();
}

