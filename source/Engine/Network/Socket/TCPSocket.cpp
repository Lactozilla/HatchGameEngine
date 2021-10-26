#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket/Socket.h>
#include <Engine/Network/Socket/Includes.h>

class TCPSocket : public Socket {
public:
    sockaddr_storage addrStorage;
};
#endif

#include <Engine/Network/Socket/Socket.h>
#include <Engine/Network/Socket/TCPSocket.h>
#include <Engine/Network/Socket/Includes.h>
#include <Engine/Network/Network.h>

PUBLIC STATIC TCPSocket* TCPSocket::New(Uint16 port, int protocol) {
    SOCKET_CHECK_PROTOCOL(NULL);

    TCPSocket* sock = new TCPSocket;
    sock->protocol = PROTOCOL_TCP;
    sock->ipProtocol = protocol;
    sock->address.port = port;
    sock->family = Socket::GetFamilyFromProtocol(protocol);

    sock->connecting = CONNSTATUS_DISCONNECTED;
    sock->server = false;
    sock->error = 0;
    memset(&sock->addrStorage, 0x00, sizeof(sockaddr_storage));

    sock->sock = socket(Socket::GetDomainFromProtocol(protocol), SOCK_STREAM, IPPROTO_TCP);
    if (sock->sock == INVALID_SOCKET) {
        SOCKET_DEBUG_ERROR("Could not open a TCP socket");
        goto fail;
    }

    if (!sock->SetNoDelay()) {
        SOCKET_DEBUG_ERROR("Could not disable Nagle's algorithm");
        sock->Close();
        goto fail;
    }

    return sock;

fail:
    delete sock;
    return NULL;
}

PUBLIC STATIC TCPSocket* TCPSocket::Open(Uint16 port, int protocol) {
    SOCKET_CHECK_PROTOCOL(NULL);
    return TCPSocket::New(port, protocol);
}

PUBLIC STATIC TCPSocket* TCPSocket::OpenClient(const char* address, Uint16 port, int protocol) {
    TCPSocket* sock = NULL;

    SOCKET_CHECK_PROTOCOL(NULL);

    if (protocol == NETADDR_ANY) {
        SOCKET_DEBUG_VERBOSE("Opening IPv6 socket");
        sock = TCPSocket::OpenClient(address, port, NETADDR_IPV6);
        if (sock) {
            return sock;
        }

        SOCKET_DEBUG_VERBOSE("Opening IPv4 socket");
        sock = TCPSocket::OpenClient(address, port, NETADDR_IPV4);
        if (sock) {
            return sock;
        }

        SOCKET_DEBUG_ERROR("Could not open any sockets");
        return NULL;
    }

    SOCKET_DEBUG_INFO("Connecting to %s, port %d", address, port);

    sock = TCPSocket::New(port, protocol);
    if (!sock) {
        SOCKET_DEBUG_ERROR("Could not create a TCP socket");
        goto fail;
    }

    if (!sock->ResolveHost(address, port)) {
        SOCKET_DEBUG_ERROR("Could not resolve host");
        goto fail;
    }

    sock->MakeSockAddr(sock->family, &sock->addrStorage);

    if (!sock->Reconnect()) {
        SOCKET_DEBUG_ERROR("Could not connect");
        goto fail;
    }

    if (!sock->SetNonBlocking()) {
        SOCKET_DEBUG_ERROR("Could not set as non-blocking");
        goto fail;
    }

    return sock;

fail:
    delete sock;
    return NULL;
}

PUBLIC bool TCPSocket::Reconnect() {
    if (connecting == CONNSTATUS_CONNECTED)
        return false;

    int connected = connect(sock, (sockaddr*)(&addrStorage), sizeof(sockaddr));

    switch (connected) {
        case 0:
            connecting = CONNSTATUS_CONNECTED;
            return true;
        case EINPROGRESS:
            connecting = CONNSTATUS_ATTEMPTING;
            return true;
        default:
            connecting = CONNSTATUS_FAILED;
            return false;
    }
}

PUBLIC STATIC TCPSocket* TCPSocket::OpenServer(Uint16 port, int protocol) {
    TCPSocket* sock = NULL;

    SOCKET_CHECK_PROTOCOL(NULL);

    if (protocol == NETADDR_ANY) {
        SOCKET_DEBUG_VERBOSE("Opening IPv6 socket");
        sock = TCPSocket::OpenServer(port, NETADDR_IPV6);
        if (sock) {
            return sock;
        }

        SOCKET_DEBUG_VERBOSE("Opening IPv4 socket");
        sock = TCPSocket::OpenServer(port, NETADDR_IPV4);
        if (sock) {
            return sock;
        }

        SOCKET_DEBUG_ERROR("Could not open any sockets");
        return NULL;
    }

    SOCKET_DEBUG_INFO("Opening server at port %d", port);

    sock = TCPSocket::New(port, protocol);
    if (!sock) {
        SOCKET_DEBUG_ERROR("Could not create a TCP socket");
        goto fail;
    }

    if (!sock->ResolveHost(Socket::GetAnyAddress(protocol), port)) {
        SOCKET_DEBUG_ERROR("Could not resolve host");
        goto fail;
    }

    if (!sock->Bind(port, sock->family)) {
        SOCKET_DEBUG_ERROR("Could not bind");
        goto fail;
    }

    if (!sock->Listen()) {
        SOCKET_DEBUG_ERROR("Could not listen");
        goto fail;
    }

    if (!sock->SetNonBlocking()) {
        SOCKET_DEBUG_ERROR("Could not set as non-blocking");
        goto fail;
    }

    sock->server = true;
    sock->SetConnected();

    return sock;

fail:
    delete sock;
    return NULL;
}

PUBLIC bool TCPSocket::Bind(Uint16 port, int family) {
    sockaddr_storage addrStorage;
    memset(&addrStorage, 0x00, sizeof(addrStorage));

    switch (family) {
        case AF_INET: {
            sockaddr_in* ipv4 = (sockaddr_in*)(&addrStorage);
            ipv4->sin_addr.s_addr = htonl(INADDR_ANY);
            ipv4->sin_port = htons(port);
            ipv4->sin_family = family;
            break;
        }
        case AF_INET6: {
            sockaddr_in6* ipv6 = (sockaddr_in6*)(&addrStorage);
            ipv6->sin6_addr = in6addr_any;
            ipv6->sin6_port = htons(port);
            ipv6->sin6_family = family;
            break;
        }
        default:
            SOCKET_DEBUG_ERROR("Unknown family");
            return false;
    }

    SOCKET_DEBUG_VERBOSE("Binding to %s", Socket::SockAddrToString(family, &addrStorage));

    if (family == AF_INET6) {
        SOCKET_DEBUG_VERBOSE("Enabling IPV6_V6ONLY");

        if (!SetIPv6Only()) {
            SOCKET_DEBUG_ERROR("Could not set TCP socket as IPv6 only");
            return false;
        }
    }

    if (bind(sock, (sockaddr*)(&addrStorage), sizeof(sockaddr)) == SOCKET_ERROR) {
        SOCKET_DEBUG_ERROR("Could not bind");
        error = socketerrno;
        return false;
    }

    socklen_t addrLength = sizeof(sockaddr);
    if (getsockname(sock, (sockaddr*)(&addrStorage), &addrLength) < 0) {
        SOCKET_DEBUG_ERROR("getsockname failed");
        error = socketerrno;
        return false;
    }
    else {
        Socket::GetAddressFromSockAddr(family, &addrStorage);
        SOCKET_DEBUG_VERBOSE("Bound to %s", Socket::SockAddrToString(family, &addrStorage));
    }

    return true;
}

PUBLIC bool TCPSocket::Listen() {
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        SOCKET_DEBUG_ERROR("Could not listen");
        return false;
    }

    SOCKET_DEBUG_VERBOSE("Now listening");

    return true;
}

PUBLIC TCPSocket* TCPSocket::Accept() {
    socklen_t addrLength = sizeof(sockaddr_storage);

    if (!server) {
        SOCKET_DEBUG_ERROR("Cannot accept as a client");
        return NULL;
    }

    TCPSocket* newSock = new TCPSocket;
    newSock->protocol = PROTOCOL_TCP;
    newSock->sock = accept(sock, (sockaddr*)(&newSock->addrStorage), &addrLength);
    if (newSock->sock == INVALID_SOCKET) {
        error = socketerrno;
        goto fail;
    }

    newSock->server = false;
    newSock->family = newSock->addrStorage.ss_family;
    newSock->error = 0;

    newSock->SetConnected();

    if (!newSock->SetNonBlocking()) {
        SOCKET_DEBUG_ERROR("Could not set new socket as non-blocking");
        goto fail;
    }

    newSock->GetAddressFromSockAddr(newSock->family, &newSock->addrStorage);

    return newSock;

fail:
    delete newSock;
    return NULL;
}

PUBLIC int TCPSocket::Send(Uint8* data, size_t length) {
    if (server) {
        SOCKET_DEBUG_ERROR("Cannot send as a server");
        return -1;
    }

    int sent = 0;
    int remaining = length;

    do {
        int len = send(sock, (const char*)data, length, 0);
        if (len >= 0) {
            remaining -= len;
            data += len;
            sent += len;
        }
        else {
            error = socketerrno;
            return SOCKET_ERROR;
        }
    } while (remaining > 0);

    return sent;
}

PUBLIC int TCPSocket::Send(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    (void)addrStorage;
    return Send(data, length);
}

PUBLIC int TCPSocket::Receive(Uint8* data, size_t length) {
    if (server) {
        SOCKET_DEBUG_ERROR("Cannot receive as a server");
        return -1;
    }

    int recvLength = 0;

    for (;;) {
        int received = recv(sock, (char*)data, length, 0);

        if (received > 0)
            recvLength += received;
        else if (received == 0)
            return SOCKET_CLOSED;
        else {
            break;
        }
    }

    if (recvLength == 0) {
        error = socketerrno;
        return SOCKET_ERROR;
    }

    return recvLength;
}

PUBLIC int TCPSocket::Receive(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    (void)addrStorage;
    return Receive(data, length);
}

PUBLIC void TCPSocket::Close() {
    if (sock != INVALID_SOCKET) {
        int error = shutdown(sock, SOCKET_SHUT_RDWR);
        if (error == ENOBUFS) {
            Log::Print(Log::LOG_ERROR, "Could not close TCP socket!");
            exit(-1);
        }

        for (;;) {
            char buf[SOCKET_BUFFER_LENGTH];
            if (recv(sock, buf, sizeof(buf), 0) <= 0)
                break;
        }

        closesocket(sock);
    }

    delete this;
}
