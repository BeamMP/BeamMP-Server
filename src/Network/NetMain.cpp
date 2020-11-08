#include "Network.h"
#include <thread>
#include <memory>
std::unique_ptr<ClientInterface> CI;
void NetMain(){
    std::thread TCP(TCPServerMain);
    TCP.detach();
    UDPServerMain();
}
