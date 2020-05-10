#include "Client.hpp"
#include <thread>

void TCPServerMain();
void UDPServerMain();

std::set<Client*> Clients;
void NetMain() {
    std::thread TCP(TCPServerMain);
    TCP.detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    UDPServerMain();
}
