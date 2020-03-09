//
// Created by Anonymous275 on 4/2/2020.
//

#define ENET_IMPLEMENTATION
#include "enet.h"
#include <string>
#include <stdio.h>
#include "../logger.h"
void ParseData(ENetPacket*packet,ENetPeer*peer); //Data Parser
void NameRequest(ENetPeer*peer);
void SendToAll(ENetHost *server,ENetEvent event);
ENetPacket* packet;


void host_server(ENetHost *server) {
    ENetEvent event;
    while (enet_host_service(server, &event, 2) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                 printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port); //Help xD
                //the data should be the client info could be name for now it's Client information
                NameRequest(event.peer);
                event.peer->Name = (void *)"Client information";
                event.peer->gameVehicleID[0] = 15;
                event.peer->serverVehicleID = 17;

                SendToAll(server,event);

                break;
            case ENET_EVENT_TYPE_RECEIVE:

                ParseData(event.packet,event.peer/*->dataLength,event.packet->data, (char *)event.peer->data, event.channelID*/); //We grab and Parse the Data
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy (event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf ("%s disconnected.\n", (char *)event.peer->Name);
                // Reset the peer's client information.
                event.peer->Name = NULL;
                break;

            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                printf ("%s timed out.\n", (char *)event.peer->Name);
                event.peer->Name = NULL;
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
    if (server == NULL) {
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
