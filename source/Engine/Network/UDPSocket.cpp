#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>

class UDPSocket : public Socket {
public:

};
#endif

#include <Engine/Network/UDPSocket.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Socket.h>

PUBLIC STATIC UDPSocket* UDPSocket::New(Uint16 port, int protocol) {
    UDPSocket* sock = NULL;

    SOCKET_CHECK_PROTOCOL(NULL);

    if (protocol == IPPROTOCOL_ANY) {
        NETWORK_DEBUG_INFO("Creating IPv6 socket");
        sock = UDPSocket::New(port, IPPROTOCOL_IPV6);
        if (sock) {
            return sock;
        }

        NETWORK_DEBUG_INFO("Creating IPv4 socket");
        sock = UDPSocket::New(port, IPPROTOCOL_IPV4);
        if (sock) {
            return sock;
        }

        NETWORK_DEBUG_ERROR("Could not create any sockets");
        return NULL;
    }

    sock = new UDPSocket;
    sock->protocol = SOCKET_UDP;
    sock->ipProtocol = protocol;
    sock->server = false;

    sock->family = Socket::GetFamilyFromProtocol(protocol);
    if (sock->family < 0) {
        NETWORK_DEBUG_ERROR("Unknown protocol");
        goto fail;
    }

    sock->sock = socket(Socket::GetDomainFromProtocol(protocol), SOCK_DGRAM, IPPROTO_UDP);
    if (sock->sock == INVALID_SOCKET) {
        NETWORK_DEBUG_ERROR("Could not open an UDP socket");
        goto fail;
    }

    sock->address.port = port;

    return sock;

fail:
    delete sock;
    return NULL;
}

PUBLIC STATIC UDPSocket* UDPSocket::Open(Uint16 port, int protocol) {
    SOCKET_CHECK_PROTOCOL(NULL);

    NETWORK_DEBUG_INFO("Opening socket with protocol %s", Socket::GetProtocolName(protocol));

    UDPSocket* socket = UDPSocket::New(port, protocol);
    if (!socket) {
        NETWORK_DEBUG_ERROR("Could not open a new socket");
        return NULL;
    }

    protocol = socket->ipProtocol;

    sockaddr_storage* addrStorage = socket->ResolveHost(Socket::GetAnyAddress(protocol), port, SOCKET_UDP, protocol);
    if (addrStorage == NULL) {
        NETWORK_DEBUG_ERROR("Could not resolve host");
        goto fail;
    }

    if (!socket->Bind(addrStorage)) {
        NETWORK_DEBUG_ERROR("Could not bind socket");
        goto fail;
    }

    if (!socket->SetNonBlocking()) {
        NETWORK_DEBUG_ERROR("Could not set socket as non-blocking");
        goto fail;
    }

    return socket;

fail:
    free(addrStorage);
    delete socket;
    return NULL;
}

PUBLIC STATIC UDPSocket* UDPSocket::OpenClient(const char* address, Uint16 port, int protocol) {
    SOCKET_CHECK_PROTOCOL(NULL);

    NETWORK_DEBUG_INFO("Connecting to %s, port %d", address, port);

    UDPSocket* socket = UDPSocket::Open(port, protocol);
    if (!socket) {
        NETWORK_DEBUG_ERROR("Could not create a new socket");
        return NULL;
    }

    if (!socket->Connect(address, port)) {
        NETWORK_DEBUG_ERROR("Could not connect");
        delete socket;
        return NULL;
    }

    return socket;
}

PUBLIC STATIC UDPSocket* UDPSocket::OpenServer(Uint16 port, int protocol) {
    SOCKET_CHECK_PROTOCOL(NULL);

    NETWORK_DEBUG_INFO("Opening server at port %d", port);

    UDPSocket* socket = UDPSocket::Open(port, protocol);
    if (!socket) {
        NETWORK_DEBUG_ERROR("Could not create a new socket");
        return NULL;
    }

    return socket;
}

PUBLIC bool UDPSocket::Bind(sockaddr_storage* addrStorage) {
    NETWORK_DEBUG_INFO("Binding to %s", Socket::SockAddrToString(family, addrStorage));

    if (bind(sock, (sockaddr*)addrStorage, sizeof(sockaddr)) == SOCKET_ERROR) {
        NETWORK_DEBUG_ERROR("Could not bind");
        return false;
    }

    int addrLength = sizeof(sockaddr);
    if (getsockname(sock, (sockaddr*)addrStorage, &addrLength) < 0) {
        NETWORK_DEBUG_ERROR("getsockname failed");
        return false;
    } else {
        Socket::SockAddrToAddress(&address, family, addrStorage);
        NETWORK_DEBUG_INFO("Bound to %s", Socket::AddressToString(protocol, &address));
    }

    return true;
}

PUBLIC bool UDPSocket::Connect(const char* address, Uint16 port) {
    addrinfo hints;
    memset(&hints, 0x00, sizeof(addrinfo));

    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;

    if (!port)
        port = SOCKET_DEFAULT_PORT;

    addrinfo* addrInfo = NULL;

    if (!getaddrinfo(address, Socket::PortToString(port), &hints, &addrInfo) && addrInfo) {
        addrinfo* curAddrInfo = addrInfo;

        while (curAddrInfo) {
            sockaddr* sa = curAddrInfo->ai_addr;
            if (!sendto(sock, NULL, 0, 0, sa, curAddrInfo->ai_addrlen)) {
                GetAddressFromSockAddr(curAddrInfo->ai_family, (sockaddr_storage*)sa);
                break;
            }
            curAddrInfo = curAddrInfo->ai_next;
        }

        freeaddrinfo(addrInfo);
    } else {
        NETWORK_DEBUG_ERROR("getaddrinfo failed");
        return false;
    }

    return true;
}

PUBLIC int UDPSocket::Send(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    int sockLen = 0;

    if (addrStorage->ss_family == AF_INET)
        sockLen = sizeof(sockaddr_in);
    else if (addrStorage->ss_family == AF_INET6)
        sockLen = sizeof(sockaddr_in6);
    else
        return -1;

    int status = sendto(sock, (char*)data, length, 0, (sockaddr*)addrStorage, sockLen);
    if (status == SOCKET_ERROR) {
        error = socketerrno;
        return status;
    }

    return length;
}

PUBLIC int UDPSocket::Receive(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    int addrLength = sizeof(sockaddr);
    int recvLength = recvfrom(sock, (char*)data, length, 0, (sockaddr*)addrStorage, &addrLength);

    if (recvLength < 0) {
        error = socketerrno;
        return SOCKET_ERROR;
    }
    else if (recvLength == 0)
        return SOCKET_CLOSED;

    return recvLength;
}
