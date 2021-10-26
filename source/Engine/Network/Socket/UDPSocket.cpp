#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket/Socket.h>
#include <Engine/Network/Socket/Includes.h>
#include <Engine/Network/Message.h>

class UDPSocket : public Socket {
public:
    addrinfo* connAddrInfo;
    MessageStorage connMessage;
};
#endif

#include <Engine/Network/Socket/Socket.h>
#include <Engine/Network/Socket/UDPSocket.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>

PUBLIC STATIC UDPSocket* UDPSocket::New(Uint16 port, int protocol) {
    UDPSocket* sock = NULL;

    SOCKET_CHECK_PROTOCOL(NULL);

    if (protocol == NETADDR_ANY) {
        SOCKET_DEBUG_VERBOSE("Creating IPv6 socket");
        sock = UDPSocket::New(port, NETADDR_IPV6);
        if (sock) {
            return sock;
        }

        SOCKET_DEBUG_VERBOSE("Creating IPv4 socket");
        sock = UDPSocket::New(port, NETADDR_IPV4);
        if (sock) {
            return sock;
        }

        SOCKET_DEBUG_ERROR("Could not create any sockets");
        return NULL;
    }

    sock = new UDPSocket;
    sock->protocol = PROTOCOL_UDP;
    sock->ipProtocol = protocol;

    sock->connecting = CONNSTATUS_DISCONNECTED;
    sock->server = false;
    sock->error = 0;
    sock->connAddrInfo = NULL;

    INIT_MESSAGE_STORAGE(sock->connMessage);

    sock->family = Socket::GetFamilyFromProtocol(protocol);
    if (sock->family < 0) {
        SOCKET_DEBUG_ERROR("Unknown protocol");
        goto fail;
    }

    sock->sock = socket(Socket::GetDomainFromProtocol(protocol), SOCK_DGRAM, IPPROTO_UDP);
    if (sock->sock == INVALID_SOCKET) {
        SOCKET_DEBUG_ERROR("Could not open an UDP socket");
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

    SOCKET_DEBUG_VERBOSE("Opening socket with protocol %s", Socket::GetProtocolName(protocol));

    UDPSocket* socket = UDPSocket::New(port, protocol);
    if (!socket) {
        SOCKET_DEBUG_ERROR("Could not open a new socket");
        return NULL;
    }

    protocol = socket->ipProtocol;

    sockaddr_storage* addrStorage = socket->ResolveHost(Socket::GetAnyAddress(protocol), port, PROTOCOL_UDP, protocol);
    if (addrStorage == NULL) {
        SOCKET_DEBUG_ERROR("Could not resolve host");
        goto fail;
    }

    if (!socket->Bind(addrStorage)) {
        SOCKET_DEBUG_ERROR("Could not bind socket");
        goto fail;
    }

    if (!socket->SetNonBlocking()) {
        SOCKET_DEBUG_ERROR("Could not set socket as non-blocking");
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

    SOCKET_DEBUG_INFO("Connecting to %s, port %d", address, port);

    UDPSocket* socket = UDPSocket::Open(port, protocol);
    if (!socket) {
        SOCKET_DEBUG_ERROR("Could not create a new socket");
        return NULL;
    }

    if (!socket->Connect(address, port)) {
        SOCKET_DEBUG_ERROR("Could not connect");
        delete socket;
        return NULL;
    }

    return socket;
}

PUBLIC STATIC UDPSocket* UDPSocket::OpenServer(Uint16 port, int protocol) {
    SOCKET_CHECK_PROTOCOL(NULL);

    SOCKET_DEBUG_INFO("Opening server at port %d", port);

    UDPSocket* socket = UDPSocket::Open(port, protocol);
    if (!socket) {
        SOCKET_DEBUG_ERROR("Could not create a new socket");
        return NULL;
    }

    return socket;
}

PUBLIC bool UDPSocket::Bind(sockaddr_storage* addrStorage) {
    SOCKET_DEBUG_VERBOSE("Binding to %s", Socket::SockAddrToString(family, addrStorage));

    if (family == AF_INET6) {
        SOCKET_DEBUG_VERBOSE("Enabling IPV6_V6ONLY");

        if (!SetIPv6Only()) {
            SOCKET_DEBUG_ERROR("Could not set UDP socket as IPv6 only");
            return false;
        }
    }

    if (bind(sock, (sockaddr*)addrStorage, sizeof(sockaddr)) == SOCKET_ERROR) {
        SOCKET_DEBUG_ERROR("Could not bind");
        error = socketerrno;
        return false;
    }

    socklen_t addrLength = sizeof(sockaddr);
    if (getsockname(sock, (sockaddr*)addrStorage, &addrLength) < 0) {
        SOCKET_DEBUG_ERROR("getsockname failed");
        error = socketerrno;
        return false;
    }
    else {
        Socket::SockAddrToAddress(&address, family, addrStorage);
        SOCKET_DEBUG_VERBOSE("Bound to %s", Socket::AddressToString(protocol, &address));
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
        connAddrInfo = addrInfo;
        Reconnect();
    }
    else {
        SOCKET_DEBUG_ERROR("getaddrinfo failed");
        connecting = CONNSTATUS_FAILED;
        connAddrInfo = NULL;
        return false;
    }

    connecting = CONNSTATUS_ATTEMPTING;

    return true;
}

PUBLIC bool UDPSocket::Reconnect() {
    addrinfo* curAddrInfo = connAddrInfo;

    while (curAddrInfo) {
        sockaddr* sa = curAddrInfo->ai_addr;

        if (!sendto(sock, (char*)connMessage.data, connMessage.length, 0, sa, curAddrInfo->ai_addrlen)) {
            GetAddressFromSockAddr(curAddrInfo->ai_family, (sockaddr_storage*)sa);
            break;
        }

        curAddrInfo = curAddrInfo->ai_next;
    }

    return true;
}

PUBLIC void UDPSocket::SetConnectionMessage(Uint8* message, size_t messageLength) {
    if (!message || !messageLength) {
        INIT_MESSAGE_STORAGE(connMessage);
        return;
    }

    if (messageLength >= SOCKET_BUFFER_LENGTH)
        messageLength = SOCKET_BUFFER_LENGTH;

    connMessage.length = messageLength;
    memcpy(connMessage.data, message, messageLength);
}

PUBLIC bool UDPSocket::SetConnected() {
    connecting = CONNSTATUS_CONNECTED;

    SetConnectionMessage(NULL, 0);

    if (connAddrInfo)
        freeaddrinfo(connAddrInfo);
    connAddrInfo = NULL;

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
    socklen_t addrLength = sizeof(sockaddr);
    int recvLength = recvfrom(sock, (char*)data, length, 0, (sockaddr*)addrStorage, &addrLength);

    if (recvLength < 0) {
        error = socketerrno;
        return SOCKET_ERROR;
    }

    return recvLength;
}
