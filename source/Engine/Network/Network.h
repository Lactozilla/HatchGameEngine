#ifndef NETWORK_H
#define NETWORK_H

#include <Engine/Includes/Standard.h>

enum {
    PROTOCOL_TCP,
    PROTOCOL_UDP
};

enum {
    NETADDR_ANY,
    NETADDR_IPV4,
    NETADDR_IPV6
};

enum {
    REFUSE_NONE,       // No reason given.
    REFUSE_NOTALLOWED, // The server is not accepting new clients.
    REFUSE_PLAYERCOUNT // There are too many players connected.
};

enum {
    CONNSTATUS_DISCONNECTED,
    CONNSTATUS_ATTEMPTING,
    CONNSTATUS_CONNECTED,
    CONNSTATUS_FAILED
};

typedef Uint8 AckNum;

typedef enum {
    ACKCMP_LESS,
    ACKCMP_EQUAL,
    ACKCMP_HIGHER
} AckCompare;

#define ACKCMP_DIFF_MIN INT8_MIN
#define ACKCMP_DIFF_MAX INT8_MAX

#define ACK_SEQUENCE_BUFFER_SIZE 64

struct PacketRetransmission {
    AckNum ackNum, ackSequence;
    Uint8 timesResent;
    int sendTo;
    double sentAt;
};

#define CONNECTION_ID_SELF -1
#define CONNECTION_ID_NOBODY -2
#define CONNECTION_ID_VALID(id) ((id) >= CONNECTION_ID_SELF)

#define MAX_NETWORK_CONNECTIONS 16
#define MAX_NETGAME_PLAYERS 8

#define MAX_CLIENT_NAME 33
#define MAX_CLIENT_COMMANDS 64
#define MAX_NETWORK_EVENT_LENGTH 256

#define MAX_MESSAGES_TO_RESEND 100
#define MAX_MESSAGE_RESEND_ATTEMPTS 30

#define NETWORK_SOCKET_TIMEOUT 15
#define NETWORK_HEARTBEAT_INTERVAL 100

#define CLIENT_TIMEOUT_SECS 10
#define CLIENT_WAITING_INTERVAL 1000
#define CLIENT_RECONNECTION_INTERVAL 1000

//#define NETWORK_DROP_PACKETS
#define NETWORK_PACKET_DROP_PERCENTAGE 50.0

#define NETWORK_DEBUG

#ifdef NETWORK_DEBUG
    #include <Engine/Diagnostics/Log.h>

    #define NETWORK_LOG_INFO_MESSAGES
    #define NETWORK_LOG_VERBOSE_MESSAGES
    #define NETWORK_LOG_ERROR_MESSAGES
    #define NETWORK_LOG_IMPORTANT_MESSAGES

    #ifdef NETWORK_LOG_INFO_MESSAGES
        #define NETWORK_DEBUG_INFO(...) \
            Log::Print(Log::LOG_INFO, "[" __FUNCTION__ "] " __VA_ARGS__)
    #else
        #define NETWORK_DEBUG_INFO
    #endif

    #ifdef NETWORK_LOG_VERBOSE_MESSAGES
        #define NETWORK_DEBUG_VERBOSE(...) \
            Log::Print(Log::LOG_VERBOSE, "[" __FUNCTION__ "] " __VA_ARGS__)
    #else
        #define NETWORK_DEBUG_VERBOSE
    #endif

    #ifdef NETWORK_LOG_IMPORTANT_MESSAGES
        #define NETWORK_DEBUG_IMPORTANT(...) \
            Log::Print(Log::LOG_IMPORTANT, "[" __FUNCTION__ "] " __VA_ARGS__)
    #else
        #define NETWORK_DEBUG_IMPORTANT
    #endif

    #ifdef NETWORK_LOG_ERROR_MESSAGES
        #define NETWORK_DEBUG_ERROR(...) \
            Log::Print(Log::LOG_ERROR, "[" __FUNCTION__ "] " __VA_ARGS__)
    #else
        #define NETWORK_DEBUG_ERROR
    #endif
#else
    #define NETWORK_DEBUG_INFO
    #define NETWORK_DEBUG_IMPORTANT
    #define NETWORK_DEBUG_ERROR
#endif

#endif /* NETWORK_H */
