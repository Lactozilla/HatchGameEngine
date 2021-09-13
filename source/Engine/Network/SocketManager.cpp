#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket/Socket.h>
#include <Engine/Network/Socket/Includes.h>

class SocketManager {
public:
    Socket* sockets[MAX_SOCKETS];
    int numSockets;
};
#endif

#include <Engine/Network/Socket/Socket.h>
#include <Engine/Network/Socket/TCPSocket.h>
#include <Engine/Network/Socket/UDPSocket.h>
#include <Engine/Network/SocketManager.h>
#include <Engine/Network/Network.h>

PUBLIC STATIC SocketManager* SocketManager::New() {
    SocketManager* sockMan = new SocketManager;

    sockMan->numSockets = 0;

    for (int i = 0; i < MAX_SOCKETS; i++)
        sockMan->sockets[i] = NULL;

    return sockMan;
}

#define CHECK_SOCKET_LIMIT \
    if (numSockets == MAX_SOCKETS) \
        return -1;

PRIVATE int SocketManager::InsertSocket(Socket* sock) {
    CHECK_SOCKET_LIMIT

    int i = 0;

    for (; i < MAX_SOCKETS; i++) {
        if (sockets[i] == NULL) {
            break;
        }
    }

    numSockets++;
    sockets[i] = sock;
    return i;
}

PUBLIC int SocketManager::OpenTCPClient(const char* address, Uint16 port) {
    CHECK_SOCKET_LIMIT

    TCPSocket* socket = TCPSocket::OpenClient(address, port, NETADDR_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC int SocketManager::OpenTCPServer(Uint16 port) {
    CHECK_SOCKET_LIMIT

    TCPSocket* socket = TCPSocket::OpenServer(port, NETADDR_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC int SocketManager::OpenUDPClient(const char* address, Uint16 port) {
    CHECK_SOCKET_LIMIT

    UDPSocket* socket = UDPSocket::OpenClient(address, port, NETADDR_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC int SocketManager::OpenUDPServer(Uint16 port) {
    CHECK_SOCKET_LIMIT

    UDPSocket* socket = UDPSocket::OpenServer(port, NETADDR_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

#undef CHECK_SOCKET_LIMIT

PUBLIC int SocketManager::AttemptToOpenTCPClient(const char* address, Uint16 port) {
    TCPSocket* socket = NULL;

    if (numSockets == MAX_SOCKETS)
        return -1;

    NETWORK_DEBUG_VERBOSE("Opening IPv4 socket");
    socket = TCPSocket::OpenClient(address, port, NETADDR_IPV4);

    if (socket) {
        return InsertSocket(socket);
    }
    else {
        NETWORK_DEBUG_VERBOSE("Could not open IPv4 socket");
    }

    NETWORK_DEBUG_VERBOSE("Opening IPv6 socket");
    socket = TCPSocket::OpenClient(address, port, NETADDR_IPV6);

    if (socket) {
        return InsertSocket(socket);
    }
    else {
        NETWORK_DEBUG_VERBOSE("Could not open IPv6 socket");
    }

    return -1;
}

PUBLIC void SocketManager::OpenMultipleUDPSockets(Uint16 port, int* start, int* end) {
    UDPSocket* socket = NULL;

    (*start) = -1;
    (*end) = -1;

    if (numSockets == MAX_SOCKETS)
        return;

    NETWORK_DEBUG_VERBOSE("Opening IPv4 socket");
    socket = UDPSocket::Open(port, NETADDR_IPV4);

    if (socket) {
        (*start) = InsertSocket(socket);
        (*end) = (*start) + 1;
    }
    else {
        NETWORK_DEBUG_VERBOSE("Could not open IPv4 socket");
    }

    if (numSockets == MAX_SOCKETS)
        return;

    NETWORK_DEBUG_VERBOSE("Opening IPv6 socket");
    socket = UDPSocket::Open(port, NETADDR_IPV6);

    if (socket) {
        (*end) = InsertSocket(socket) + 1;
    }
    else {
        NETWORK_DEBUG_VERBOSE("Could not open IPv6 socket");
    }
}

#define CHECK_SOCKET(ret) \
    if (sock < 0 || sock >= numSockets) \
        return ret

#define GET_SOCKET(ret) \
    Socket* socket = GetSocket(sock); \
    if (socket == NULL) \
        return ret

#define CLIENT_ONLY(ret) \
    if (socket->protocol == PROTOCOL_TCP && socket->server) \
        return ret

PUBLIC Socket* SocketManager::GetSocket(int sock) {
    CHECK_SOCKET(NULL);
    return sockets[sock];
}

#define REMOVE_SOCKET \
    sockets[sock] = NULL; \
    if (numSockets) \
        numSockets--;

PUBLIC void SocketManager::RemoveSocket(int sock) {
    GET_SOCKET();

    socket->Dispose();
    REMOVE_SOCKET
}

PUBLIC void SocketManager::CloseSocket(int sock) {
    GET_SOCKET();

    socket->Close();
    REMOVE_SOCKET
}

#undef REMOVE_SOCKET

PUBLIC int SocketManager::GetProtocol(int sock) {
    GET_SOCKET(-1);
    return socket->protocol;
}

PUBLIC int SocketManager::GetIPProtocol(int sock) {
    GET_SOCKET(-1);
    return socket->ipProtocol;
}

PUBLIC SocketAddress* SocketManager::GetAddress(int sock) {
    GET_SOCKET(NULL);
    return &socket->address;
}

PUBLIC int SocketManager::GetError(int sock) {
    GET_SOCKET(-1);
    return socket->error;
}

PUBLIC int SocketManager::Accept(int sock) {
    GET_SOCKET(-1);

    if (socket->protocol != PROTOCOL_TCP)
        return -1;

    Socket* newSock = socket->Accept();
    if (newSock) {
        return InsertSocket(newSock);
    }

    return -1;
}

PUBLIC bool SocketManager::Connect(int sock, const char* address, Uint16 port) {
    GET_SOCKET(false);

    if (socket->protocol != PROTOCOL_UDP)
        return false;

    return socket->Connect(address, port);
}

PUBLIC bool SocketManager::Reconnect(int sock) {
    GET_SOCKET(false);
    return socket->Reconnect();
}

PUBLIC bool SocketManager::SetConnectionMessage(int sock, Uint8* message, size_t messageLength) {
    GET_SOCKET(false);

    if (socket->protocol != PROTOCOL_UDP)
        return false;

    socket->SetConnectionMessage(message, messageLength);
    return true;
}

PUBLIC int SocketManager::GetConnectionStatus(int sock) {
    GET_SOCKET(-1);
    return socket->connecting;
}

PUBLIC bool SocketManager::SetConnected(int sock) {
    GET_SOCKET(false);
    return socket->SetConnected();
}

#define SOCKET_CHECK_OR_RETURN_NULL \
    GET_SOCKET(NULL); \
    CLIENT_ONLY(NULL);

#define SOCKET_CHECK_OR_RETURN_ZERO \
    GET_SOCKET(0); \
    CLIENT_ONLY(0);

#define SOCKET_CHECK_OR_RETURN_FALSE \
    GET_SOCKET(false); \
    CLIENT_ONLY(false);

#define SOCKET_CHECK_SEND_OR_RECEIVE \
    GET_SOCKET(-1); \
    CLIENT_ONLY(-1);

#define SOCKET_SEND(d, l) \
    if (socket->protocol == PROTOCOL_UDP) \
        return -1; \
    return socket->Send(d, l)

#define SOCKET_SEND_ADDR(d, l) \
    if (socket->protocol == PROTOCOL_UDP) { \
        sockaddr_storage addrStorage;  \
        if (!socket->AddressToSockAddr(sockAddress, sockAddress->protocol, &addrStorage)) \
            return -1; \
        return socket->Send(d, l, &addrStorage); \
    } \
    else if (socket->protocol == PROTOCOL_TCP) \
        return socket->Send(d, l); \
    else \
        return -1;

#define SOCKET_RECEIVE(d, l) \
    if (socket->protocol == PROTOCOL_UDP) { \
        sockaddr_storage addrStorage; \
        return socket->Receive(d, l, &addrStorage); \
    } \
    return socket->Receive(d, l);

#define SOCKET_RECEIVE_ADDR(d, l) \
    if (socket->protocol == PROTOCOL_UDP) { \
        sockaddr_storage addrStorage; \
        int received = socket->Receive(d, l, &addrStorage); \
        if (received >= 0) { \
            Socket::SockAddrToAddress(sockAddress, addrStorage.ss_family, &addrStorage); \
        } \
        return received; \
    } \
    return socket->Receive(d, l);

#define SOCKET_RECEIVE_RETURN(d, l, success, fail) \
    if (socket->protocol == PROTOCOL_UDP) { \
        sockaddr_storage addrStorage; \
        if (socket->Receive(d, l, &addrStorage) < 0) \
            return fail; \
    } \
    else if (socket->Receive(d, l) < 0) \
        return fail; \
    return success;

#define SOCKET_RECEIVE_ADDR_RETURN(d, l, success, fail) \
    if (socket->protocol == PROTOCOL_UDP) { \
        sockaddr_storage addrStorage; \
        int received = socket->Receive(d, l, &addrStorage); \
        if (received >= 0) { \
            Socket::SockAddrToAddress(sockAddress, addrStorage.ss_family, &addrStorage); \
        } \
        else \
            return fail; \
    } \
    else if (socket->Receive(d, l) < 0) \
        return fail; \
    return success; \

PUBLIC int SocketManager::Send(int sock, Uint8* data, size_t length) {
    SOCKET_CHECK_SEND_OR_RECEIVE
    SOCKET_SEND(data, length);
}

PUBLIC int SocketManager::Send(int sock, Uint8* data, size_t length, SocketAddress* sockAddress) {
    SOCKET_CHECK_SEND_OR_RECEIVE
    SOCKET_SEND_ADDR(data, length);
}

PUBLIC int SocketManager::Receive(int sock, Uint8* data, size_t length) {
    SOCKET_CHECK_SEND_OR_RECEIVE
    SOCKET_RECEIVE(data, length);
}

PUBLIC int SocketManager::Receive(int sock, Uint8* data, size_t length, SocketAddress* sockAddress) {
    SOCKET_CHECK_SEND_OR_RECEIVE
    SOCKET_RECEIVE_ADDR(data, length);
}

PUBLIC int SocketManager::SendNumber(int sock, int number) {
    SOCKET_CHECK_OR_RETURN_FALSE
    SOCKET_SEND((Uint8*)(&number), sizeof(number));
}

PUBLIC int SocketManager::SendNumber(int sock, int number, SocketAddress* sockAddress) {
    SOCKET_CHECK_OR_RETURN_FALSE
    SOCKET_SEND_ADDR((Uint8*)(&number), sizeof(number));
}

#define SOCKET_RECEIVE_NUMBER(macro) \
    int number = 0; \
    macro((Uint8*)(&number), sizeof(number), number, 0);

PUBLIC int SocketManager::ReceiveNumber(int sock) {
    SOCKET_CHECK_OR_RETURN_ZERO
    SOCKET_RECEIVE_NUMBER(SOCKET_RECEIVE_RETURN);
}

PUBLIC int SocketManager::ReceiveNumber(int sock, SocketAddress* sockAddress) {
    SOCKET_CHECK_OR_RETURN_ZERO
    SOCKET_RECEIVE_NUMBER(SOCKET_RECEIVE_ADDR_RETURN);
}

#undef SOCKET_RECEIVE_NUMBER

PUBLIC int SocketManager::SendString(int sock, char* string) {
    SOCKET_CHECK_OR_RETURN_FALSE
    SOCKET_SEND((Uint8*)string, strlen(string)+1);
}

PUBLIC int SocketManager::SendString(int sock, char* string, SocketAddress* sockAddress) {
    SOCKET_CHECK_OR_RETURN_FALSE
    SOCKET_SEND_ADDR((Uint8*)string, strlen(string)+1);
}

#define SOCKET_RECEIVE_STRING(macro) \
    char* string = (char*)calloc(sizeof(char), maxLength); \
    macro((Uint8*)string, maxLength, string, NULL);

PUBLIC char* SocketManager::ReceiveString(int sock, int maxLength) {
    SOCKET_CHECK_OR_RETURN_NULL
    SOCKET_RECEIVE_STRING(SOCKET_RECEIVE_RETURN);
}

PUBLIC char* SocketManager::ReceiveString(int sock, int maxLength, SocketAddress* sockAddress) {
    SOCKET_CHECK_OR_RETURN_NULL
    SOCKET_RECEIVE_STRING(SOCKET_RECEIVE_ADDR_RETURN);
}

#undef SOCKET_RECEIVE_STRING

#undef CHECK_SOCKET
#undef GET_SOCKET
#undef CLIENT_ONLY

#undef SOCKET_CHECK_OR_RETURN_NULL
#undef SOCKET_CHECK_OR_RETURN_ZERO
#undef SOCKET_CHECK_OR_RETURN_FALSE
#undef SOCKET_CHECK_SEND_OR_RECEIVE

#undef SOCKET_SEND
#undef SOCKET_SEND_ADDR

#undef SOCKET_RECEIVE
#undef SOCKET_RECEIVE_ADDR
#undef SOCKET_RECEIVE_RETURN
#undef SOCKET_RECEIVE_ADDR_RETURN

PUBLIC void SocketManager::Dispose() {
    for (int i = 0; i < numSockets; i++) {
        if (sockets[i])
            sockets[i]->Close();
    }

    delete this;
}
