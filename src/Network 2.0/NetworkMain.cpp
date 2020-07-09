#include "Client.hpp"
#include <thread>

void TCPServerMain();
void UDPServerMain();
void SLoop();
std::set<Client*> Clients;
void NetMain() {
    std::thread TCP(TCPServerMain);
    TCP.detach();
    std::thread Sec(SLoop);
    Sec.detach();
    UDPServerMain();
}
