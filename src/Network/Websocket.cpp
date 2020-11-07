///
/// Created by Anonymous275 on 11/6/2020
///
/*#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>*/
#include "Security/Enc.h"
#include <iostream>
#include "Logger.h"
#include <thread>
#include <string>

/*namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::string GetRes(const beast::flat_buffer& buff) {
    return (char*)buff.data().data();
}*/

void SyncData(){
    /*DebugPrintTID();
    try {
        std::string const host = Sec("95.216.35.232");

        net::io_context ioc;
        tcp::resolver r(ioc);

        websocket::stream<tcp::socket> ws(ioc);
        auto const results = r.resolve(host, Sec("3600"));
        net::connect(ws.next_layer(), results.begin(), results.end());


        ws.handshake(host, "/");
        beast::flat_buffer buffer;
        ws.write(boost::asio::buffer("Hello, world!"));
        ws.read(buffer);

        std::cout << GetRes(buffer) << std::endl;

        ws.close(websocket::close_code::normal);

    }catch(std::exception const& e){
        error(e.what());
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(0);
    }*/
}


void WebsocketInit(){
    /*std::thread t1(SyncData);
    t1.detach();*/
}

