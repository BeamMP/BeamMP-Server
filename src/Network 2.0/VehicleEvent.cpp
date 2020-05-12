///
/// Created by Anonymous275 on 5/9/2020
///
///TCP

#include "Client.hpp"
#include <iostream>
#include <WS2tcpip.h>
#include "../logger.h"
#include "../Settings.hpp"

void CreateNewThread(Client*client);
void CreateClient(SOCKET TCPSock){
    auto *client = new Client;
    client->SetTCPSock(TCPSock);
    Clients.insert(client);
    CreateNewThread(client);
}

void TCPServerMain(){

    WSADATA wsaData;
    if (WSAStartup(514, &wsaData)) //2.2
    {
        std::cout << "Can't start Winsock!" << std::endl;
        return;
    }
    SOCKET client, Listener = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_addr.S_un.S_addr = ADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);


    if (bind(Listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << "Can't bind socket! " << WSAGetLastError() << std::endl;
        return;
    }

    if(Listener == -1)
    {
        printf("Invalid socket");
        return;
    }

    if(listen(Listener,SOMAXCONN))
    {
        std::cout << "listener failed " << GetLastError();
        return;
    }
    info("Vehicle event network online");
    do{
        client = accept(Listener, nullptr, nullptr);
        if(client == -1)
        {
            std::cout << "invalid client socket" << std::endl;
            continue;
        }
        if(Clients.size() < MaxPlayers)CreateClient(client);
    }while(client);

    closesocket(client);
    WSACleanup();
}