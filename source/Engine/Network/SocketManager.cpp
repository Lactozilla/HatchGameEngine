#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>

class SocketManager {
public:
    vector<Socket*> sockets;
};
#endif

#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketManager.h>
#include <Engine/Network/TCPSocket.h>
#include <Engine/Network/UDPSocket.h>
#include <Engine/Network/Network.h>

PUBLIC STATIC SocketManager* SocketManager::New() {
    return new SocketManager;
}

PUBLIC int SocketManager::NumSockets() {
    return sockets.size();
}

PRIVATE int SocketManager::InsertSocket(Socket* sock) {
    for (int i = 0; i < NumSockets(); i++) {
        if (sockets[i] == NULL) {
            sockets[i] = sock;
            return i;
        }
    }

    sockets.push_back(sock);
    return sockets.size() - 1;
}

PUBLIC int SocketManager::OpenTCPClient(const char* address, Uint16 port) {
    TCPSocket* socket = TCPSocket::OpenClient(address, port, IPPROTOCOL_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC int SocketManager::OpenTCPServer(Uint16 port) {
    TCPSocket* socket = TCPSocket::OpenServer(port, IPPROTOCOL_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC int SocketManager::OpenUDPClient(const char* address, Uint16 port) {
    UDPSocket* socket = UDPSocket::OpenClient(address, port, IPPROTOCOL_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC int SocketManager::OpenUDPServer(Uint16 port) {
    UDPSocket* socket = UDPSocket::OpenServer(port, IPPROTOCOL_ANY);
    if (socket) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC void SocketManager::OpenUDPSockets(Uint16 port, int* start, int* end) {
    UDPSocket* socket = NULL;

    (*start) = -1;
    (*end) = -1;

    NETWORK_DEBUG_INFO("Opening IPv4 socket");
    socket = UDPSocket::Open(port, IPPROTOCOL_IPV4);

    if (socket) {
        (*start) = InsertSocket(socket);
        (*end) = (*start) + 1;
    } else {
        NETWORK_DEBUG_INFO("Could not open IPv4 socket");
    }

    NETWORK_DEBUG_INFO("Opening IPv6 socket");
    socket = UDPSocket::Open(port, IPPROTOCOL_IPV6);

    if (socket) {
        (*end) = InsertSocket(socket) + 1;
    } else {
        NETWORK_DEBUG_INFO("Could not open IPv6 socket");
    }
}

#define CHECK_SOCKET(ret) \
    if (sock < 0 || sock >= NumSockets()) \
        return ret

#define GET_SOCKET(ret) \
    Socket* socket = GetSocket(sock); \
    if (socket == NULL) \
        return ret

#define CLIENT_ONLY(ret) \
    if (socket->protocol == SOCKET_TCP && socket->server) \
        return ret

PUBLIC Socket* SocketManager::GetSocket(int sock) {
    CHECK_SOCKET(NULL);
    return sockets[sock];
}

PUBLIC void SocketManager::CloseSocket(int sock) {
    GET_SOCKET();

    socket->Close();
    sockets[sock] = NULL;
}

PUBLIC int SocketManager::GetProtocol(int sock) {
    GET_SOCKET(-1);
    return socket->protocol;
}

PUBLIC int SocketManager::GetIPProtocol(int sock) {
    GET_SOCKET(-1);
    return socket->protocol;
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

    if (socket->protocol != SOCKET_TCP)
        return -1;

    Socket* newSock = socket->Accept();
    if (newSock) {
        return InsertSocket(socket);
    }

    return -1;
}

PUBLIC bool SocketManager::Connect(int sock, const char* address, Uint16 port) {
    GET_SOCKET(false);

    if (socket->protocol != SOCKET_UDP)
        return false;

    return socket->Connect(address, port);
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
    if (socket->protocol == SOCKET_UDP) \
        return -1; \
    return socket->Send(d, l)

#define SOCKET_SEND_ADDR(d, l) \
    if (socket->protocol == SOCKET_UDP) { \
        sockaddr_storage addrStorage;  \
        if (!socket->AddressToSockAddr(sockAddress, sockAddress->protocol, &addrStorage)) \
            return -1; \
        return socket->Send(d, l, &addrStorage); \
    } else if (socket->protocol == SOCKET_TCP) \
        return socket->Send(d, l); \
    else \
        return -1;

#define SOCKET_RECEIVE(d, l) \
    if (socket->protocol == SOCKET_UDP) { \
        sockaddr_storage addrStorage; \
        return socket->Receive(d, l, &addrStorage); \
    } \
    return socket->Receive(d, l);

#define SOCKET_RECEIVE_ADDR(d, l) \
    if (socket->protocol == SOCKET_UDP) { \
        sockaddr_storage addrStorage; \
        int received = socket->Receive(d, l, &addrStorage); \
        if (received > -1) { \
            socket->SockAddrToAddress(sockAddress, addrStorage.ss_family, &addrStorage); \
        } \
        return received; \
    } \
    return socket->Receive(d, l);

#define SOCKET_RECEIVE_RETURN(d, l, success, fail) \
    if (socket->protocol == SOCKET_UDP) { \
        sockaddr_storage addrStorage; \
        if (socket->Receive(d, l, &addrStorage) < 0) \
            return fail; \
    } else if (socket->Receive(d, l) < 0) \
        return fail; \
    return success;

#define SOCKET_RECEIVE_ADDR_RETURN(d, l, success, fail) \
    if (socket->protocol == SOCKET_UDP) { \
        sockaddr_storage addrStorage; \
        int received = socket->Receive(d, l, &addrStorage); \
        if (received > -1) { \
            socket->SockAddrToAddress(sockAddress, addrStorage.ss_family, &addrStorage); \
        } else \
            return fail; \
    } else if (socket->Receive(d, l) < 0) \
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
    for (int i = 0; i < NumSockets(); i++)
        sockets[i]->Close();
    delete this;
}
