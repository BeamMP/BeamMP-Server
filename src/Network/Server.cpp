#define ENET_IMPLEMENTATION
#include "enet.h"
#include <stdio.h>
void ParseData(size_t Length, enet_uint8* Data, char* Sender, enet_uint8 ChannelID); //Data Parser
void host_server(ENetHost *server) {
    ENetEvent event;
    while (enet_host_service(server, &event, 2) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("new Client Connected ::1:%u.\n", event.peer->address.port);

                //the data should be the client info could be name for now it's Client information
                event.peer->data = (void *)"Client information";
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                ParseData(event.packet->dataLength,event.packet->data, (char *)event.peer->data, event.channelID); //We grab and Parse the Data
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy (event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf ("%s disconnected.\n", (char *)event.peer->data);
                // Reset the peer's client information.
                event.peer->data = NULL;
                break;

            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                printf ("%s timeout.\n", (char *)event.peer->data);
                event.peer->data = NULL;
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

    address.host = ENET_HOST_ANY; /* Bind the server to the default localhost. */
    address.port = Port; /* Bind the server to port 7777. */


    /* create a server */
    printf("starting server with a maximum of %d Clients...\n",MaxClients);
    server = enet_host_create(&address, MaxClients, 2, 0, 0);
    if (server == NULL) {
        printf("An error occurred while trying to create a server host.\n");
        return;
    }

    printf("Waiting for clients on port %d...\n",Port);

    while (true) {
        host_server(server);
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return;
}

