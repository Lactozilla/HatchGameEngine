#ifndef ENGINE_NETWORK_NETGAMEMANAGER_H
#define ENGINE_NETWORK_NETGAMEMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Network/ConnectionManager.h>

class NetgameManager {
private:
    static bool Receive();

public:
    static ConnectionManager* Connection;
    static bool Started;
    static bool Server;
    static int Protocol;

    static bool Start();
    static bool InitProtocol(int protocol);
    static bool StartServer(int protocol, Uint16 port);
    static bool StartServer(Uint16 port);
    static bool StartServer();
    static bool StartUDPServer(Uint16 port);
    static bool StartUDPServer();
    static bool StartTCPServer(Uint16 port);
    static bool StartTCPServer();
    static bool Connect(int protocol, const char* address, Uint16 port);
    static bool Connect(const char* address, Uint16 port);
    static bool Connect(const char* address);
    static bool Connect(Uint16 port);
    static bool Connect();
    static bool ConnectUDP(const char* address, Uint16 port);
    static bool ConnectUDP(const char* address);
    static bool ConnectUDP(Uint16 port);
    static bool ConnectUDP();
    static bool ConnectTCP(const char* address, Uint16 port);
    static bool ConnectTCP(const char* address);
    static bool ConnectTCP(Uint16 port);
    static bool ConnectTCP();
    static bool Stop();
    static void Update();
};

#endif /* ENGINE_NETWORK_NETGAMEMANAGER_H */
