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
#define MAX_CLIENT_COMMANDS 16
#define MAX_NETWORK_EVENT_LENGTH 256

#define MAX_MESSAGES_TO_RESEND 100
#define MAX_MESSAGE_RESEND_ATTEMPTS 30

#define NETWORK_SOCKET_TIMEOUT 15
#define NETWORK_HEARTBEAT_INTERVAL 100

#define CLIENT_TIMEOUT_SECS 10
#define CLIENT_WAITING_INTERVAL 1000
#define CLIENT_RECONNECTION_INTERVAL 1000

#define INPUT_BUFFER_FRAMES 60
#define MESSAGE_INPUT_BUFFER_FRAMES INPUT_BUFFER_FRAMES

// Pack this NetworkCommands struct.
#if defined(_MSC_VER)
    #pragma pack(push, 1)
#endif

struct NetworkCommands {
    Uint8 data[MAX_CLIENT_COMMANDS];
    Uint8 numCommands;
} STRUCT_PACK;

// Unpack this NetworkCommands struct.
#if defined(_MSC_VER)
    #pragma pack(pop)
#endif

// #define NETWORK_DROP_PACKETS
#define NETWORK_PACKET_DROP_PERCENTAGE 50.0

#define NETWORK_DEBUG

#ifdef NETWORK_DEBUG
    #include <Engine/Diagnostics/Log.h>

    #define NETWORK_LOG_INFO_MESSAGES
    #define NETWORK_LOG_VERBOSE_MESSAGES
    #define NETWORK_LOG_IMPORTANT_MESSAGES
    // #define NETWORK_LOG_FRAME_MESSAGES
    #define NETWORK_LOG_WARNING_MESSAGES
    #define NETWORK_LOG_ERROR_MESSAGES

    #if defined(_MSC_VER)
        #define NETWORK_DEBUG_FUNCNAME __FUNCTION__
    #else
        inline char* GetClassAndMethodName(const char *fn)
        {
            std::string s = fn;
            int end = s.find("(");
            int start = s.substr(0, end).rfind(" ") + 1;

            static char funcname[512];
            strncpy(funcname, s.substr(start, end - start).c_str(), sizeof(funcname));

            return funcname;
        }

        #define NETWORK_DEBUG_FUNCNAME GetClassAndMethodName(__PRETTY_FUNCTION__)
    #endif

    inline char *VariadicFormat(const char *format, ...)
    {
        static char buf[8192];
        va_list vargs;

        va_start(vargs, format);
        vsnprintf(buf, sizeof(buf), format, vargs);
        va_end(vargs);

        return buf;
    }

    #define NETWORK_LOG_FORMAT "[%s] %s"

    #ifdef NETWORK_LOG_INFO_MESSAGES
        #define NETWORK_DEBUG_INFO(...) \
            Log::Print(Log::LOG_INFO, NETWORK_LOG_FORMAT, NETWORK_DEBUG_FUNCNAME, VariadicFormat(__VA_ARGS__))
    #else
        #define NETWORK_DEBUG_INFO
    #endif

    #ifdef NETWORK_LOG_VERBOSE_MESSAGES
        #define NETWORK_DEBUG_VERBOSE(...) \
            Log::Print(Log::LOG_VERBOSE, NETWORK_LOG_FORMAT, NETWORK_DEBUG_FUNCNAME, VariadicFormat(__VA_ARGS__))
    #else
        #define NETWORK_DEBUG_VERBOSE
    #endif

    #ifdef NETWORK_LOG_IMPORTANT_MESSAGES
        #define NETWORK_DEBUG_IMPORTANT(...) \
            Log::Print(Log::LOG_IMPORTANT, NETWORK_LOG_FORMAT, NETWORK_DEBUG_FUNCNAME, VariadicFormat(__VA_ARGS__))
    #else
        #define NETWORK_DEBUG_IMPORTANT
    #endif

    #ifdef NETWORK_LOG_FRAME_MESSAGES
        #define NETWORK_DEBUG_FRAME(...) \
            Log::Print(Log::LOG_VERBOSE, NETWORK_LOG_FORMAT, NETWORK_DEBUG_FUNCNAME, VariadicFormat(__VA_ARGS__))
    #else
        #define NETWORK_DEBUG_FRAME
    #endif

    #ifdef NETWORK_LOG_WARNING_MESSAGES
        #define NETWORK_DEBUG_WARNING(...) \
            Log::Print(Log::LOG_WARN, NETWORK_LOG_FORMAT, NETWORK_DEBUG_FUNCNAME, VariadicFormat(__VA_ARGS__))
    #else
        #define NETWORK_DEBUG_WARNING
    #endif

    #ifdef NETWORK_LOG_ERROR_MESSAGES
        #define NETWORK_DEBUG_ERROR(...) \
            Log::Print(Log::LOG_ERROR, NETWORK_LOG_FORMAT, NETWORK_DEBUG_FUNCNAME, VariadicFormat(__VA_ARGS__))
    #else
        #define NETWORK_DEBUG_ERROR
    #endif
#else
    #define NETWORK_DEBUG_INFO
    #define NETWORK_DEBUG_VERBOSE
    #define NETWORK_DEBUG_IMPORTANT
    #define NETWORK_DEBUG_FRAME
    #define NETWORK_DEBUG_WARNING
    #define NETWORK_DEBUG_ERROR
#endif

#endif /* NETWORK_H */
