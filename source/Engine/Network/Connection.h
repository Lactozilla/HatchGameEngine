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

    NetworkCommands commands[INPUT_BUFFER_FRAMES];
    bool receivedCommands[INPUT_BUFFER_FRAMES];
};

struct ServerClient {
    Uint8 State;
    Uint8 PlayerID;

    char Name[MAX_CLIENT_NAME];
    double Timeout;
    double CommandTimeout;
    double JoinTimeout;
    Uint8 ReadyResent;

    Uint32 Frame, NextFrame;
};

#endif /* NETWORKCONNECTION_H */
