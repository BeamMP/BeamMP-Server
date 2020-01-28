//
// Created by jojos38 on 28.01.2020.
//

#include <iostream>
#include "network.h"
#include "logger.h"
using namespace std;
#define EXIT_SUCCESS /*implementation defined*/
#define EXIT_FAILURE /*implementation defined*/

void listen();

ENetAddress address;
ENetHost* server;
ENetEvent event;

void startRUDP(char host[], int port) {
    // ---------- Initializing ENet ---------- //
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);
    info("ENet initialized");

    // ---------- Starting server ---------- //
    enet_address_set_host(&address, host); // Set host
    address.port = port; // Set port
    server = enet_host_create(
        &address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);
    if (server == NULL) {
        fprintf(stderr, "An error occurred while trying to create an ENet server host.\n");
        return EXIT_FAILURE;
    }
    info("Server started");
    listen();
}

void listen() {
    info("Listening for packets...");
    while (true) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                /* Store any relevant client information here. */
                event.peer->data = "Client information";
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                printf("A packet of length %u containing %s was received from %s on channel %u.\n",
                    event.packet->dataLength,
                    event.packet->data,
                    event.peer->data,
                    event.channelID);
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                printf("%s disconnected.\n", event.peer->data);
                /* Reset the peer's client information. */
                event.peer->data = NULL;
        }
    }
}