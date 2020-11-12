#include "Network.h"
#include <memory>
#include <thread>
std::unique_ptr<ClientInterface> CI;
void NetMain() {
    std::thread TCP(TCPServerMain);
    TCP.detach();
    UDPServerMain();
}
