#ifndef ENGINE_NETWORK_SOCKETMANAGER_H
#define ENGINE_NETWORK_SOCKETMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>

class SocketManager {
private:
    int InsertSocket(Socket* sock);

public:
    vector<Socket*> sockets;

    static SocketManager* New();
    int NumSockets();
    int OpenTCPClient(const char* address, Uint16 port);
    int OpenTCPServer(Uint16 port);
    int OpenUDPClient(const char* address, Uint16 port);
    int OpenUDPServer(Uint16 port);
    void OpenUDPSockets(Uint16 port, int* start, int* end);
    Socket* GetSocket(int sock);
    void CloseSocket(int sock);
    int GetProtocol(int sock);
    int GetIPProtocol(int sock);
    SocketAddress* GetAddress(int sock);
    int GetError(int sock);
    int Accept(int sock);
    bool Connect(int sock, const char* address, Uint16 port);
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
