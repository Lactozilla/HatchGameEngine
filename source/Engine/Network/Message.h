#ifndef MESSAGE_H
#define MESSAGE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Socket/Includes.h>

#define MESSAGE_RESEND_TIMEOUT 500

enum {
    MESSAGE_JOIN,   // Client wants to join
    MESSAGE_ACCEPT, // Server accepted connection
    MESSAGE_REFUSE, // Server refused connection
    MESSAGE_READY,  // Client or server is ready
    MESSAGE_QUIT,   // Disconnection

    MESSAGE_COMMANDS,
    MESSAGE_SERVERCOMMANDS,

    MESSAGE_PLAYERJOIN,
    MESSAGE_PLAYERLEAVE,
    MESSAGE_NAMECHANGE,

    MESSAGE_EVENT,

    MESSAGE_HEARTBEAT,
    MESSAGE_ACKNOWLEDGEMENT
};

#define MESSAGE_HEADER_LENGTH 7 // offsetof(struct Message, data)
#define MESSAGE_DATA_LENGTH (SOCKET_BUFFER_LENGTH - MESSAGE_HEADER_LENGTH)

struct Message {
    Uint32 checksum;
    Uint8 type;

    struct {
        AckNum sequence;
        AckNum lastReceived;
    } ack;

    union {
        // Information about the server
        struct {
            Uint8 numPlayers;
            Uint8 playerInGame[MAX_NETGAME_PLAYERS];
            char playerNames[MAX_NETGAME_PLAYERS][MAX_CLIENT_NAME];
        } serverInfo;

        // Information about the client
        struct {
            char name[MAX_CLIENT_NAME];
        } clientInfo;

        // Ready response from the server
        // The client can also send a ready message, but it won't have any info.
        struct {
            Uint8 playerID;
            char playerName[MAX_CLIENT_NAME];
        } readyInfo;

        // Why can't the client join?
        struct {
            Uint8 reason;
        } refuseInfo;

        // Player join information
        struct {
            Uint8 id;
            char name[MAX_CLIENT_NAME];
        } playerJoin;

        // Player leave information
        struct {
            Uint8 id;
        } playerLeave;

        // Commands from the player
        struct {
            Uint8 data[MAX_CLIENT_COMMANDS];
            Uint8 length;
        } clientCommands;

        // Event
        struct {
            Uint8 type;
            Uint8 data[MAX_NETWORK_EVENT_LENGTH];
            Uint8 length;
        } event;

        // Commands from the server, for all players
        struct {
            Uint8 data[MAX_NETGAME_PLAYERS][MAX_CLIENT_COMMANDS];
            Uint8 length[MAX_NETGAME_PLAYERS];
            Uint8 numCommands;
        } serverCommands;

        // Name change
        struct {
            Uint8 playerID;
            char name[MAX_CLIENT_NAME];
        } nameChange;

        // Acknowledgements
        struct {
            AckNum numAcks;
            AckNum list[ACK_SEQUENCE_BUFFER_SIZE];
        } ackList;
    };
};

struct MessageStorage {
    Uint8 data[SOCKET_BUFFER_LENGTH];
    int length;
};

#define INIT_MESSAGE_STORAGE(messageStorage) \
    memset(messageStorage.data, 0x00, sizeof(messageStorage.data)); \
    messageStorage.length = 0

#define INIT_MESSAGE(messageVar, ty) \
    Message messageVar; \
    memset(&messageVar, 0x00, sizeof(messageVar)); \
    messageVar.type = ty; \
    Packet packetToSend; \
    packetToSend.Init()

#define STORE_MESSAGE_IN_PACKET(messageVar) packetToSend.message = messageVar

#define MESSAGE_CLASS_CALL(func, messageVar, to, size, useAck) STORE_MESSAGE_IN_PACKET(messageVar); \
    packetToSend.func(to, size, useAck)

#define MESSAGE_WRITE_CALL(messageVar, to, size, useAck) MESSAGE_CLASS_CALL(PrepareMessage, messageVar, to, size, useAck)
#define MESSAGE_SEND_CALL(messageVar, to, size, useAck) MESSAGE_CLASS_CALL(Send, messageVar, to, size, useAck)

#define WRITE_MESSAGE(messageVar, field, to) MESSAGE_WRITE_CALL(messageVar, to, sizeof(messageVar.field), true)
#define WRITE_MESSAGE_NOACK(messageVar, field, to) MESSAGE_WRITE_CALL(messageVar, to, sizeof(messageVar.field), false)

#define SEND_MESSAGE(messageVar, field, to) MESSAGE_SEND_CALL(messageVar, to, sizeof(messageVar.field), true)
#define SEND_MESSAGE_NOACK(messageVar, field, to) MESSAGE_SEND_CALL(messageVar, to, sizeof(messageVar.field), false)

#define SEND_MESSAGE_ZEROLENGTH(messageVar, to) MESSAGE_SEND_CALL(messageVar, to, 0, true)

#define INIT_AND_SEND_MESSAGE_CALL(ty, to, useAck) { \
    INIT_MESSAGE(messageToSend, ty); \
    STORE_MESSAGE_IN_PACKET(messageToSend); \
    packetToSend.Send(to, 0, useAck); \
}

#define INIT_AND_SEND_MESSAGE(ty, to) INIT_AND_SEND_MESSAGE_CALL(ty, to, true)
#define INIT_AND_SEND_MESSAGE_NOACK(ty, to) INIT_AND_SEND_MESSAGE_CALL(ty, to, false)

#endif /* MESSAGE_H */
