#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/SocketIncludes.h>

class Socket {
public:
    int protocol;
    int ipProtocol, family;

    SocketAddress address;
    bool server;

    socket_t sock;
    int error;
};
#endif

#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>
#include <Engine/Network/Network.h>

PUBLIC STATIC bool Socket::IsProtocolValid(int protocol) {
    switch (protocol) {
        case IPPROTOCOL_ANY:
        case IPPROTOCOL_IPV4:
        case IPPROTOCOL_IPV6:
            return true;
    }

    return false;
}

PUBLIC STATIC int Socket::GetFamilyFromProtocol(int protocol) {
    switch (protocol) {
        case IPPROTOCOL_IPV4:
            return AF_INET;
        case IPPROTOCOL_IPV6:
            return AF_INET6;
        default:
            return -1;
    }
}

PUBLIC STATIC int Socket::GetDomainFromProtocol(int protocol) {
    switch (protocol) {
        case IPPROTOCOL_IPV4:
            return PF_INET;
        case IPPROTOCOL_IPV6:
            return PF_INET6;
        default:
            return -1;
    }
}

PUBLIC STATIC int Socket::GetProtocolFromFamily(int family) {
    switch (family) {
        case AF_INET:
            return IPPROTOCOL_IPV4;
        case AF_INET6:
            return IPPROTOCOL_IPV6;
        default:
            return -1;
    }
}

PUBLIC STATIC char* Socket::GetProtocolName(int protocol) {
    switch (protocol) {
        case IPPROTOCOL_IPV4:
            return "IPv4";
        case IPPROTOCOL_IPV6:
            return "IPv6";
        default:
            return "?";
    }
}

PUBLIC STATIC char* Socket::GetAnyAddress(int protocol) {
    switch (protocol) {
        case IPPROTOCOL_IPV6:
            return "::";
        default:
            return "0.0.0.0";
    }
}

PUBLIC bool Socket::AddressToSockAddr(SocketAddress* sockAddress, int protocol, sockaddr_storage* sa) {
    switch (protocol) {
        case IPPROTOCOL_IPV4: {
            sockaddr_in* sockaddr = (sockaddr_in*)sa;
            sockaddr->sin_addr.s_addr = (Uint32)(htonl(sockAddress->host.ipv4));
            sockaddr->sin_port = (Uint16)(htons(sockAddress->port));
            sockaddr->sin_family = AF_INET;
            return true;
        }
        case IPPROTOCOL_IPV6: {
            sockaddr_in6* sockaddr = (sockaddr_in6*)sa;
            memcpy(sockaddr->sin6_addr.s6_addr, sockAddress->host.ipv6.addr8, sizeof(sockAddress->host.ipv6.addr8));
            sockaddr->sin6_port = (Uint16)(htons(sockAddress->port));
            sockaddr->sin6_family = AF_INET6;
            return true;
        }
        default:
            return false;
    }
}

PUBLIC int Socket::SockAddrToAddress(SocketAddress* sockAddress, int family, sockaddr_storage* sa) {
    switch (family) {
        case AF_INET: {
            sockaddr_in* sockaddr = (sockaddr_in*)sa;
            sockAddress->host.ipv4 = (Uint32)ntohl(sockaddr->sin_addr.s_addr);
            sockAddress->port = (Uint16)ntohs(sockaddr->sin_port);
            sockAddress->protocol = IPPROTOCOL_IPV4;
            break;
        }
        case AF_INET6: {
            sockaddr_in6* sockaddr = (sockaddr_in6*)sa;
            memcpy(sockAddress->host.ipv6.addr8, sockaddr->sin6_addr.s6_addr, sizeof(sockaddr->sin6_addr.s6_addr));
            sockAddress->port = (Uint16)ntohs(sockaddr->sin6_port);
            sockAddress->protocol = IPPROTOCOL_IPV6;
            break;
        }
        default:
            return -1;
    }

    return sockAddress->protocol;
}

PUBLIC void Socket::MakeSockAddr(int family, sockaddr_storage* sa) {
    memset(sa, 0x00, sizeof(sockaddr_storage));
    AddressToSockAddr(&address, Socket::GetProtocolFromFamily(family), sa);
}

PUBLIC bool Socket::MakeHints(addrinfo* hints, int type, int protocol) {
    memset(hints, 0x00, sizeof(addrinfo));

    hints->ai_flags = AI_PASSIVE;

    switch (type) {
        case SOCKET_TCP:
            hints->ai_protocol = IPPROTO_TCP;
            hints->ai_socktype = SOCK_STREAM;
            break;
        case SOCKET_UDP:
            hints->ai_protocol = IPPROTO_UDP;
            hints->ai_socktype = SOCK_DGRAM;
            break;
        default:
            return false;
    }

    hints->ai_family = Socket::GetFamilyFromProtocol(protocol);

    return true;
}

PUBLIC bool Socket::GetAddressFromSockAddr(int family, sockaddr_storage* sa) {
    int protocol = Socket::SockAddrToAddress(&address, family, sa);
    if (protocol == -1)
        return false;

    ipProtocol = protocol;
    return true;
}

PUBLIC STATIC char* Socket::PortToString(Uint16 port) {
    static char addrInfoPort[6];
    snprintf(addrInfoPort, sizeof(addrInfoPort), "%d", port);
    addrInfoPort[5] = '\0';
    return addrInfoPort;
}

PRIVATE STATIC sockaddr_storage* Socket::AllocSockAddr(sockaddr_storage* sa) {
    size_t len = sizeof(sockaddr_storage);
    sockaddr_storage* addrStorage = (sockaddr_storage*)malloc(len);

    if (addrStorage == NULL) {
        NETWORK_DEBUG_ERROR("Couldn't allocate a sockaddr_storage for Socket::ResolveAddrInfo");
        return NULL;
    }

    memcpy(addrStorage, sa, len);
    return addrStorage;
}

PUBLIC sockaddr_storage* Socket::ResolveAddrInfo(const char* hostIn, Uint16 portIn, addrinfo* hints) {
    sockaddr_storage* addrStorage = nullptr;
    addrinfo* addrInfo = NULL;

    NETWORK_DEBUG_INFO("Resolving %s, port %d", hostIn, portIn);

    if (!getaddrinfo(hostIn, Socket::PortToString(portIn), hints, &addrInfo) && addrInfo) {
        addrinfo* curAddrInfo = addrInfo;

        while (curAddrInfo) {
            int family = curAddrInfo->ai_family;

            if (family == hints->ai_family) {
                sockaddr_storage* sa = (sockaddr_storage*)curAddrInfo->ai_addr;

                if (GetAddressFromSockAddr(family, sa)) {
                    addrStorage = Socket::AllocSockAddr(sa);
                    break;
                }
            }

            curAddrInfo = curAddrInfo->ai_next;
        }

        freeaddrinfo(addrInfo);
    } else {
        NETWORK_DEBUG_ERROR("getaddrinfo failed");
    }

    return addrStorage;
}

PUBLIC sockaddr_storage* Socket::ResolveHost(const char* hostIn, Uint16 portIn) {
    addrinfo hints;

    if (!Socket::MakeHints(&hints, protocol, ipProtocol)) {
        return nullptr;
    }

    return ResolveAddrInfo(hostIn, portIn, &hints);
}

PUBLIC sockaddr_storage* Socket::ResolveHost(const char* hostIn, Uint16 portIn, int type, int ipProto) {
    addrinfo hints;

    if (!Socket::MakeHints(&hints, type, ipProto)) {
        return nullptr;
    }

    return ResolveAddrInfo(hostIn, portIn, &hints);
}

PUBLIC STATIC bool Socket::CompareAddresses(SocketAddress* addrA, SocketAddress* addrB) {
    if (addrA->protocol != addrB->protocol)
        return false;

    if (addrA->protocol == IPPROTOCOL_IPV4) {
        return (addrA->host.ipv4 == addrB->host.ipv4);
    } else if (addrA->protocol == IPPROTOCOL_IPV6) {
        if (!memcmp(&addrA->host.ipv6.addr8, &addrB->host.ipv6.addr8, sizeof(addrA->host.ipv6.addr8)))
            return true;
    }

    return false;
}

PUBLIC STATIC bool Socket::CompareSockAddr(sockaddr_storage* addrA, sockaddr_storage* addrB) {
    if (addrA->ss_family == AF_INET) {
        sockaddr_in* addrA_in = (sockaddr_in*)(addrA);
        sockaddr_in* addrB_in = (sockaddr_in*)(addrB);
        return (addrA_in->sin_addr.s_addr == addrB_in->sin_addr.s_addr);
    } else if (addrA->ss_family == AF_INET6) {
        sockaddr_in6* addrA_in6 = (sockaddr_in6*)(addrA);
        sockaddr_in6* addrB_in6 = (sockaddr_in6*)(addrB);
        if (!memcmp(&addrA_in6->sin6_addr, &addrB_in6->sin6_addr, sizeof(addrA_in6->sin6_addr)))
            return true;
    }

    return false;
}

#define IP_ADDR_LENGTH 21
#define FULL_ADDR_LENGTH IP_ADDR_LENGTH+6
#define UNKNOWN_ADDRESS ""

PRIVATE STATIC char* Socket::FormatAddress(int family, void* addr, int port) {
    static char ipAddress[IP_ADDR_LENGTH];
    static char addrString[FULL_ADDR_LENGTH];

    inet_ntop(family, addr, ipAddress, IP_ADDR_LENGTH);
    addrString[0] = '\0';

    switch (family) {
        case AF_INET:
            snprintf(addrString, FULL_ADDR_LENGTH, "%s:%d", ipAddress, port);
            break;
        case AF_INET6:
            snprintf(addrString, FULL_ADDR_LENGTH, "[%s]:%d", ipAddress, port);
            break;
        default:
            return UNKNOWN_ADDRESS;
    }

    return addrString;
}

#undef IP_ADDR_LENGTH
#undef FULL_ADDR_LENGTH

PUBLIC STATIC char* Socket::AddressToString(int protocol, SocketAddress* sockAddr) {
    int family = Socket::GetFamilyFromProtocol(protocol);
    if (family < 0) {
        return UNKNOWN_ADDRESS;
    }

    void* addr = NULL;
    in_addr addrIPv4;
    in6_addr addrIPv6;

    switch (protocol) {
        case IPPROTOCOL_IPV4:
            addrIPv4.s_addr = htonl(sockAddr->host.ipv4);
            addr = (void*)(&addrIPv4);
            break;
        case IPPROTOCOL_IPV6:
            memcpy(&addrIPv6.s6_addr, sockAddr->host.ipv6.addr8, sizeof(sockAddr->host.ipv6.addr8));
            addr = (void*)(&addrIPv6);
            break;
        default:
            return UNKNOWN_ADDRESS;
    }

    return Socket::FormatAddress(family, addr, sockAddr->port);
}

PUBLIC STATIC char* Socket::SockAddrToString(int family, sockaddr_storage* addrStorage) {
    void* addr = NULL;
    Uint16 port = 0;

    switch (family) {
        case AF_INET:
            addr = (void*)(&((sockaddr_in*)addrStorage)->sin_addr);
            port = (Uint16)ntohs(((sockaddr_in*)addrStorage)->sin_port);
            break;
        case AF_INET6:
            addr = (void*)(&((sockaddr_in6*)addrStorage)->sin6_addr);
            port = (Uint16)ntohs(((sockaddr_in6*)addrStorage)->sin6_port);
            break;
        default:
            return UNKNOWN_ADDRESS;
    }

    return Socket::FormatAddress(family, addr, port);
}

#undef UNKNOWN_ADDRESS

PUBLIC void Socket::SetNoDelay() {
    if (protocol != SOCKET_TCP)
        return;

    const int noDelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)(&noDelay), sizeof(noDelay));
}

static bool SetBlockingState(socket_t sock, bool block) {
#ifdef USING_WINSOCK
    unsigned long state = block ? 0 : 1;
    if (ioctlsocket((SOCKET)sock, FIONBIO, &state))
        return false;
#elif defined (O_NONBLOCK)
    int flags = fcntl(sock, F_GETFL, 0);
    if (block)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;
    fcntl(sock, F_SETFL, flags);
#endif
    return true;
}

PUBLIC bool Socket::SetBlocking() {
    return SetBlockingState(sock, true);
}

PUBLIC bool Socket::SetNonBlocking() {
    return SetBlockingState(sock, false);
}

PUBLIC void Socket::Close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    delete this;
}

// New
PUBLIC STATIC Socket* Socket::New(Uint16 port, int protocol) {
    (void)port;
    (void)protocol;
    return nullptr;
}

// Open
PUBLIC STATIC Socket* Socket::Open(Uint16 port, int protocol) {
    (void)port;
    (void)protocol;
    return nullptr;
}

// Open server
PUBLIC STATIC Socket* Socket::OpenServer(Uint16 port, int protocol) {
    (void)port;
    (void)protocol;
    return nullptr;
}

// Open client
PUBLIC STATIC Socket* Socket::OpenClient(const char* address, Uint16 port, int protocol) {
    (void)address;
    (void)port;
    (void)protocol;
    return nullptr;
}

// Connect
PUBLIC VIRTUAL bool Socket::Connect(const char* address, Uint16 port) {
    (void)address;
    (void)port;
    return false;
}

// Bind
PUBLIC VIRTUAL bool Socket::Bind(sockaddr_storage* addrStorage) {
    return false;
}

PUBLIC VIRTUAL bool Socket::Bind(Uint16 port, int family) {
    return false;
}

// Listen
PUBLIC VIRTUAL bool Socket::Listen() {
    return false;
}

// Accept
PUBLIC VIRTUAL Socket* Socket::Accept() {
    return nullptr;
}

// Send
PUBLIC VIRTUAL int Socket::Send(Uint8* data, size_t length) {
    (void)data;
    (void)length;
    return -1;
}

PUBLIC VIRTUAL int Socket::Send(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    (void)data;
    (void)length;
    (void)addrStorage;
    return -1;
}

// Receive
PUBLIC VIRTUAL int Socket::Receive(Uint8* data, size_t length) {
    (void)data;
    (void)length;
    return -1;
}

PUBLIC VIRTUAL int Socket::Receive(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    (void)data;
    (void)length;
    (void)addrStorage;
    return -1;
}
