#ifndef ENGINE_NETWORK_CONNECTIONMANAGER_H
#define ENGINE_NETWORK_CONNECTIONMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class SocketManager;

#include <Engine/Includes/Standard.h>
#include <Engine/Network/SocketIncludes.h>
#include <Engine/Network/SocketManager.h>
#include <Engine/Network/NetworkClient.h>

class ConnectionManager {
public:
    SocketManager* sockManager;
    vector<NetworkClient*> clients;
    vector<int> sockets;
    bool init, started;
    bool server, tcpMode;
    int networkClient;
    bool newClient;
    Uint8 buffer[SOCKET_BUFFER_LENGTH];
    int dataLength;

    static ConnectionManager* New();
    bool SetUDP();
    bool SetTCP();
    bool Init(Uint16 port);
    bool StartUDP(Uint16 port);
    bool StartTCP(Uint16 port);
    bool Start(Uint16 port);
    bool StartUDPServer(Uint16 port);
    bool StartTCPServer(Uint16 port);
    bool StartServer(Uint16 port);
    bool Connect(const char* address, Uint16 port);
    int NumClients();
    int CreateClient();
    void RemoveClient(int clientNum);
    int ReceiveUDP();
    int ReceiveTCP();
    int Receive();
    bool Send(int clientNum);
    bool Send();
    char* GetAddress(int clientNum);
    char* GetAddress();
    void Dispose();
};

#endif /* ENGINE_NETWORK_CONNECTIONMANAGER_H */
