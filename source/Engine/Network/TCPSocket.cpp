#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>

class TCPSocket : public Socket {
public:

};
#endif

#include <Engine/Network/TCPSocket.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Socket.h>

PUBLIC STATIC TCPSocket* TCPSocket::New(Uint16 port, int protocol) {
    SOCKET_CHECK_PROTOCOL(NULL);

    TCPSocket* sock = new TCPSocket;
    sock->protocol = SOCKET_TCP;
    sock->ipProtocol = protocol;
    sock->address.port = port;
    sock->family = Socket::GetFamilyFromProtocol(protocol);
    sock->server = false;

    sock->sock = socket(Socket::GetDomainFromProtocol(protocol), SOCK_STREAM, IPPROTO_TCP);
    if (sock->sock == INVALID_SOCKET) {
        NETWORK_DEBUG_ERROR("Could not open a TCP socket");
        goto fail;
    }

    sock->SetNoDelay();

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

    if (protocol == IPPROTOCOL_ANY) {
        NETWORK_DEBUG_INFO("Opening IPv6 socket");
        sock = TCPSocket::OpenClient(address, port, IPPROTOCOL_IPV6);
        if (sock) {
            return sock;
        }

        NETWORK_DEBUG_INFO("Opening IPv4 socket");
        sock = TCPSocket::OpenClient(address, port, IPPROTOCOL_IPV4);
        if (sock) {
            return sock;
        }

        NETWORK_DEBUG_ERROR("Could not open any sockets");
        return NULL;
    }

    sock = TCPSocket::New(port, protocol);
    if (!sock) {
        NETWORK_DEBUG_ERROR("Could not create a TCP socket");
        goto fail;
    }

    if (!sock->ResolveHost(address, port)) {
        NETWORK_DEBUG_ERROR("Could not resolve host");
        goto fail;
    }

    sockaddr_storage addrStorage;
    sock->MakeSockAddr(sock->family, &addrStorage);

    int connected = connect(sock->sock, (sockaddr*)(&addrStorage), sizeof(sockaddr));
    if (connected == SOCKET_ERROR) {
        NETWORK_DEBUG_ERROR("Could not connect");
        goto fail;
    }

    if (!sock->SetNonBlocking()) {
        NETWORK_DEBUG_ERROR("Could not set as non-blocking");
        goto fail;
    }

    return sock;

fail:
    delete sock;
    return NULL;
}

PUBLIC STATIC TCPSocket* TCPSocket::OpenServer(Uint16 port, int protocol) {
    TCPSocket* sock = NULL;

    SOCKET_CHECK_PROTOCOL(NULL);

    if (protocol == IPPROTOCOL_ANY) {
        NETWORK_DEBUG_INFO("Opening IPv6 socket");
        sock = TCPSocket::OpenServer(port, IPPROTOCOL_IPV6);
        if (sock) {
            return sock;
        }

        NETWORK_DEBUG_INFO("Opening IPv4 socket");
        sock = TCPSocket::OpenServer(port, IPPROTOCOL_IPV4);
        if (sock) {
            return sock;
        }

        NETWORK_DEBUG_ERROR("Could not open any sockets");
        return NULL;
    }

    sock = TCPSocket::New(port, protocol);
    if (!sock) {
        NETWORK_DEBUG_ERROR("Could not create a TCP socket");
        goto fail;
    }

    if (!sock->ResolveHost(Socket::GetAnyAddress(protocol), port)) {
        NETWORK_DEBUG_ERROR("Could not resolve host");
        goto fail;
    }

    if (!sock->Bind(port, sock->family)) {
        NETWORK_DEBUG_ERROR("Could not bind");
        goto fail;
    }

    if (!sock->Listen()) {
        NETWORK_DEBUG_ERROR("Could not listen");
        goto fail;
    }

    if (!sock->SetNonBlocking()) {
        NETWORK_DEBUG_ERROR("Could not set as non-blocking");
        goto fail;
    }

    sock->server = true;

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
            ipv4->sin_addr.s_addr = INADDR_ANY;
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
            NETWORK_DEBUG_ERROR("Unknown family");
            return false;
    }

    NETWORK_DEBUG_INFO("Binding to %s", Socket::SockAddrToString(family, &addrStorage));

    if (bind(sock, (sockaddr*)(&addrStorage), sizeof(sockaddr)) == SOCKET_ERROR) {
        NETWORK_DEBUG_ERROR("Could not bind");
        return false;
    }

    int addrLength = sizeof(sockaddr);
    if (getsockname(sock, (sockaddr*)(&addrStorage), &addrLength) < 0) {
        NETWORK_DEBUG_ERROR("getsockname failed");
        return false;
    } else {
        Socket::GetAddressFromSockAddr(family, &addrStorage);
        NETWORK_DEBUG_INFO("Bound to %s", Socket::SockAddrToString(family, &addrStorage));
    }

    return true;
}

PUBLIC bool TCPSocket::Listen() {
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        NETWORK_DEBUG_ERROR("Could not listen");
        return false;
    }

    NETWORK_DEBUG_INFO("Now listening");

    return true;
}

PUBLIC TCPSocket* TCPSocket::Accept() {
    sockaddr_storage addrStorage;
    int addrLength = sizeof(sockaddr);

    if (!server) {
        NETWORK_DEBUG_ERROR("Cannot accept as a server");
        return NULL;
    }

    TCPSocket* newSock = new TCPSocket;
    newSock->protocol = SOCKET_TCP;
    newSock->sock = accept(sock, (sockaddr*)(&addrStorage), &addrLength);
    if (newSock->sock == INVALID_SOCKET) {
        goto fail;
    }

    newSock->server = false;
    newSock->family = addrStorage.ss_family;

    if (!newSock->SetBlocking()) {
        NETWORK_DEBUG_ERROR("Could not set socket as blocking");
        goto fail;
    }

    newSock->GetAddressFromSockAddr(newSock->family, &addrStorage);

    return newSock;

fail:
    delete newSock;
    return NULL;
}

PUBLIC int TCPSocket::Send(Uint8* data, size_t length) {
    if (server) {
        NETWORK_DEBUG_ERROR("Cannot send as a server");
        return -1;
    }

    int sent = 0;

    do {
        int len = send(sock, (const char*)data, length, 0);
        if (len > 0) {
            length -= len;
            data += len;
            sent += len;
        } else
            break;
    } while (length > 0);

    return sent;
}

PUBLIC int TCPSocket::Send(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    (void)addrStorage;
    return Send(data, length);
}

PUBLIC int TCPSocket::Receive(Uint8* data, size_t length) {
    if (server) {
        NETWORK_DEBUG_ERROR("Cannot receive as a server");
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
            error = socketerrno;
            return SOCKET_ERROR;
        }
    }

    return recvLength;
}

PUBLIC int TCPSocket::Receive(Uint8* data, size_t length, sockaddr_storage* addrStorage) {
    (void)addrStorage;
    return Receive(data, length);
}
