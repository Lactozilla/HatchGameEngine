#ifndef SOCKETINCLUDES_H
#define SOCKETINCLUDES_H

struct SocketAddress {
    int protocol;
    Uint16 port;
    union {
        Uint32 ipv4;
        union {
            Uint8 addr8[16];
            Uint16 addr16[8];
            Uint32 addr32[4];
        } ipv6;
    } host;
};

#define MAX_SOCKETS 1024
#define SOCKET_DEFAULT_PORT 4845
#define SOCKET_BUFFER_LENGTH 1452

#define SOCKET_CHECK_PROTOCOL(ret) \
    if (!Socket::IsProtocolValid(protocol)) { \
        NETWORK_DEBUG_ERROR("Unknown protocol %d", protocol); \
        return ret; \
    }

#define SOCKET_DEBUG

#ifdef SOCKET_DEBUG
    #define SOCKET_DEBUG_INFO NETWORK_DEBUG_INFO
    #define SOCKET_DEBUG_VERBOSE NETWORK_DEBUG_VERBOSE
    #define SOCKET_DEBUG_ERROR NETWORK_DEBUG_ERROR
#else
    #define SOCKET_DEBUG_INFO
    #define SOCKET_DEBUG_VERBOSE
    #define SOCKET_DEBUG_ERROR
#endif

#ifdef _WIN32
    #if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
        #define _CRT_SECURE_NO_WARNINGS // _CRT_SECURE_NO_WARNINGS for sscanf errors in MSVC2013 Express
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <fcntl.h>
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/types.h>
    #include <io.h>
    #ifndef _SSIZE_T_DEFINED
        typedef int ssize_t;
        #define _SSIZE_T_DEFINED
    #endif
    #ifndef _SOCKET_T_DEFINED
        typedef SOCKET socket_t;
        #define _SOCKET_T_DEFINED
    #endif
    #ifndef snprintf
        #define snprintf _snprintf_s
    #endif
    #if _MSC_VER >=1600
        // vs2010 or later
        #include <stdint.h>
    #else
        typedef __int8 int8_t;
        typedef unsigned __int8 uint8_t;
        typedef __int32 int32_t;
        typedef unsigned __int32 uint32_t;
        typedef __int64 int64_t;
        typedef unsigned __int64 uint64_t;
    #endif
    #define socketerrno WSAGetLastError()
    #define SOCKET_EAGAIN_EINPROGRESS WSAEINPROGRESS
    #define SOCKET_ECONNREFUSED WSAECONNREFUSED
    #define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
    #define SOCKET_SHUT_RDWR SD_BOTH
    #define USING_WINSOCK
#else
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
    #ifndef _SOCKET_T_DEFINED
        typedef int socket_t;
        #define _SOCKET_T_DEFINED
    #endif
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET (-1)
    #endif
    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR   (-1)
    #endif
    #define closesocket(s) close(s)
    #include <errno.h>
    #define socketerrno errno
    #define SOCKET_EAGAIN_EINPROGRESS EAGAIN
    #define SOCKET_ECONNREFUSED ECONNREFUSED
    #define SOCKET_EWOULDBLOCK EWOULDBLOCK
    #define SOCKET_SHUT_RDWR SHUT_RDWR
#endif

#define SOCKET_CLOSED INT_MIN

#include <vector>

#endif /* SOCKETINCLUDES_H */
