#include "Network.h"
#include <thread>
ClientInterface* CI;
void NetMain(){
    std::thread TCP(TCPServerMain);
    TCP.detach();
    UDPServerMain();
}
