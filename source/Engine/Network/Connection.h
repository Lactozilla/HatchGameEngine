#ifndef NETWORKCONNECTION_H
#define NETWORKCONNECTION_H

#include <Engine/Includes/Standard.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Socket/Includes.h>

struct NetworkConnection {
    int socket;
    SocketAddress address;

    double lastTimeReceived;

    struct {
        AckNum sequence;
        AckNum lastReceived, lastSeqAcknowledged;

        struct {
            AckNum head, tail;
            AckNum ring[ACK_SEQUENCE_BUFFER_SIZE];
        } queue;

        double lastTimeSent;
    } ack;
};

struct NetworkPlayer {
    bool ingame;
    int client;
    char name[MAX_CLIENT_NAME];
    char pendingName[MAX_CLIENT_NAME];

    Uint8 data[MAX_CLIENT_COMMANDS];
    int numCommands;
};

struct ServerClient {
    Uint8 State;
    char Name[MAX_CLIENT_NAME];
    double Timeout;
    double JoinTimeout;
    Uint8 ReadyResent;
    Uint8 PlayerID;
};

#endif /* NETWORKCONNECTION_H */
