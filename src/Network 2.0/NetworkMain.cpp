#include "Client.hpp"
#include <thread>

void TCPServerMain();
void UDPServerMain();

std::set<Client*> Clients;
void NetMain() {
    std::thread TCP(TCPServerMain);
    TCP.detach();
    UDPServerMain();
}
