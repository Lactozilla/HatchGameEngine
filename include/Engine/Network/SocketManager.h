#ifndef ENGINE_NETWORK_SOCKETMANAGER_H
#define ENGINE_NETWORK_SOCKETMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Socket;

#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket/Socket.h>
#include <Engine/Network/Socket/Includes.h>

class SocketManager {
private:
    int InsertSocket(Socket* sock);

public:
    Socket* sockets[MAX_SOCKETS];
    int numSockets;

    static SocketManager* New();
    int OpenTCPClient(const char* address, Uint16 port);
    int OpenTCPServer(Uint16 port);
    int OpenUDPClient(const char* address, Uint16 port);
    int OpenUDPServer(Uint16 port);
    int AttemptToOpenTCPClient(const char* address, Uint16 port);
    void OpenMultipleUDPSockets(Uint16 port, int* start, int* end);
    Socket* GetSocket(int sock);
    void RemoveSocket(int sock);
    void CloseSocket(int sock);
    int GetProtocol(int sock);
    int GetIPProtocol(int sock);
    SocketAddress* GetAddress(int sock);
    int GetError(int sock);
    int Accept(int sock);
    bool Connect(int sock, const char* address, Uint16 port);
    bool Reconnect(int sock);
    bool SetConnectionMessage(int sock, Uint8* message, size_t messageLength);
    int GetConnectionStatus(int sock);
    bool SetConnected(int sock);
    int Send(int sock, Uint8* data, size_t length);
    int Send(int sock, Uint8* data, size_t length, SocketAddress* sockAddress);
    int Receive(int sock, Uint8* data, size_t length);
    int Receive(int sock, Uint8* data, size_t length, SocketAddress* sockAddress);
    int SendNumber(int sock, int number);
    int SendNumber(int sock, int number, SocketAddress* sockAddress);
    int ReceiveNumber(int sock);
    int ReceiveNumber(int sock, SocketAddress* sockAddress);
    int SendString(int sock, char* string);
    int SendString(int sock, char* string, SocketAddress* sockAddress);
    char* ReceiveString(int sock, int maxLength);
    char* ReceiveString(int sock, int maxLength, SocketAddress* sockAddress);
    void Dispose();
};

#endif /* ENGINE_NETWORK_SOCKETMANAGER_H */
