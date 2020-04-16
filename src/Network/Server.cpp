///
/// Created by Anonymous275 on 4/2/2020
///

#define ENET_IMPLEMENTATION
#include "enet.h"
#include <string>
#include <cstdio>
#include "../logger.h"
#include "../Settings.hpp"

void ParseData(ENetPacket*packet,ENetPeer*peer); //Data Parser
void OnConnect(ENetPeer*peer);

ENetPacket* packet;
int PlayerCount = 0;

int FindID(ENetHost *server,ENetPeer*peer){
    int OpenID = 1;
    bool Found;
    do {
        Found = true;
        for (int i = 0; i < server->connectedPeers; i++) {
            if (&server->peers[i] != peer) {
                if(server->peers[i].serverVehicleID == OpenID){
                    Found = false;
                    OpenID++;
                    break;
                }
            }
        }
    }while (!Found);
    return OpenID;
}


void host_server(ENetHost *server) {
    ENetEvent event;
    PlayerCount = server->connectedPeers;
    while (enet_host_service(server, &event, 2) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                 printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                //the data should be the client info could be name for now it's Client information
                event.peer->Name = (void *)"Client information";
                event.peer->gameVehicleID[0] = 0;
                event.peer->serverVehicleID = FindID(server, event.peer);
                OnConnect(event.peer);
                break;

            case ENET_EVENT_TYPE_RECEIVE:

                ParseData(event.packet,event.peer);
                /*->dataLength,event.packet->data, (char *)event.peer->data, event.channelID*/ //We grab and Parse the Data
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy (event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf ("%s disconnected.\n", (char *)event.peer->Name);
                // Reset the peer's client information.
                event.peer->Name = nullptr;
                break;

            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                printf ("%s timed out.\n", (char *)event.peer->Name);
                event.peer->Name = nullptr;
                break;

            case ENET_EVENT_TYPE_NONE: break;
        }
    }
}

void ServerMain(int Port, int MaxClients) {

    if (enet_initialize() != 0) {
        printf("An error occurred while initializing.\n");
        return;
    }

    ENetHost *server;
    ENetAddress address = {0};

    address.host = ENET_HOST_ANY; //Bind the server to the default localhost.
    address.port = Port; // Sets the port

    //create a server
    info("starting server with a maximum of " + to_string(MaxClients) + " Clients...");
    server = enet_host_create(&address, MaxClients, 2, 0, 0);
    if (server == nullptr) {
        error("An error occurred while trying to create a server host.");
        return;
    }

    info("Waiting for clients on port "+to_string(Port)+"...");

    while (true) {
        host_server(server);
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return;
}


void CreateNewThread(void*);

void TCPMain(int Port){
    info("Starting TCP Server on port " + to_string(Port));

    WSADATA wsaData;
    int iResult;
    sockaddr_in addr{};
    SOCKET sock,client;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);

    iResult = WSAStartup(MAKEWORD(2,2),&wsaData);

    if(iResult)
    {
        printf("WSA startup failed");
        return;
    }


    sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    if(sock == INVALID_SOCKET)
    {
        printf("Invalid socket");
        return;
    }

    iResult = bind(sock,(sockaddr*)&addr,sizeof(sockaddr_in ));

    if(iResult)
    {

        printf("bind failed %lu",GetLastError());

        return;
    }

    iResult = listen(sock,SOMAXCONN);

    if(iResult)
    {

        printf("iResult failed %lu",GetLastError());

        return;
    }

    while(client = accept(sock,nullptr,nullptr))
    {
        if(client == INVALID_SOCKET)
        {
            printf("invalid client socket\n");
            continue;
        }
        CreateNewThread((void*)&client);
    }
}