#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/ConnectionManager.h>

class NetgameManager {
public:
    static ConnectionManager* Connection;
    static bool Started;
    static bool Server;
    static int Protocol;
};
#endif

#include <Engine/Network/Network.h>
#include <Engine/Network/NetgameManager.h>
#include <Engine/Network/ConnectionManager.h>

ConnectionManager* NetgameManager::Connection = NULL;
bool               NetgameManager::Started = false;
bool               NetgameManager::Server = false;
int                NetgameManager::Protocol = NETGAME_UDP;

PUBLIC STATIC bool NetgameManager::Start() {
    if (NetgameManager::Started) {
        NETWORK_DEBUG_ERROR("Already started");
        return false;
    }

    NetgameManager::Connection = ConnectionManager::New();
    if (NetgameManager::Connection == NULL) {
        NETWORK_DEBUG_ERROR("Could not create ConnectionManager");
        return false;
    }

    return true;
}

PUBLIC STATIC bool NetgameManager::InitProtocol(int protocol) {
    if (NetgameManager::Connection == NULL) {
        NETWORK_DEBUG_ERROR("No ConnectionManager");
        return false;
    }

    if (protocol == NETGAME_UDP) {
        NETWORK_DEBUG_INFO("UDP mode");

        if (!NetgameManager::Connection->SetUDP()) {
            NETWORK_DEBUG_ERROR("Could not set UDP mode");
            return false;
        }
    }
    else if (protocol == NETGAME_TCP) {
        NETWORK_DEBUG_INFO("TCP mode");

        if (!NetgameManager::Connection->SetTCP()) {
            NETWORK_DEBUG_ERROR("Could not set TCP mode");
            return false;
        }
    }
    else {
        NETWORK_DEBUG_ERROR("Unknown protocol set");
        return false;
    }

    NetgameManager::Protocol = protocol;

    return true;
}

PUBLIC STATIC bool NetgameManager::StartServer(int protocol, Uint16 port) {
    NETWORK_DEBUG_INFO("Starting");

    if (!NetgameManager::Start()) {
        NETWORK_DEBUG_ERROR("Could not start NetgameManager");
        return false;
    }

    if (!NetgameManager::InitProtocol(protocol)) {
        NETWORK_DEBUG_ERROR("Could not initialize protocol");
        return false;
    }

    if (!NetgameManager::Connection->StartServer(port)) {
        NETWORK_DEBUG_ERROR("Could not start server");
        return false;
    }

    NetgameManager::Started = true;
    NetgameManager::Server = true;

    return true;
}

// Server methods
PUBLIC STATIC bool NetgameManager::StartServer(Uint16 port) {
    return NetgameManager::StartServer(NETGAME_UDP, port);
}

PUBLIC STATIC bool NetgameManager::StartServer() {
    return NetgameManager::StartServer(NETGAME_UDP, SOCKET_DEFAULT_PORT);
}

// UDP server methods
PUBLIC STATIC bool NetgameManager::StartUDPServer(Uint16 port) {
    return NetgameManager::StartServer(port);
}

PUBLIC STATIC bool NetgameManager::StartUDPServer() {
    return NetgameManager::StartServer();
}

// TCP server methods
PUBLIC STATIC bool NetgameManager::StartTCPServer(Uint16 port) {
    return NetgameManager::StartServer(NETGAME_TCP, port);
}

PUBLIC STATIC bool NetgameManager::StartTCPServer() {
    return NetgameManager::StartServer(NETGAME_TCP, SOCKET_DEFAULT_PORT);
}

PUBLIC STATIC bool NetgameManager::Connect(int protocol, const char* address, Uint16 port) {
    NETWORK_DEBUG_INFO("Starting");

    if (!NetgameManager::Start()) {
        NETWORK_DEBUG_ERROR("Could not start NetgameManager");
        return false;
    }

    if (!NetgameManager::InitProtocol(protocol)) {
        NETWORK_DEBUG_ERROR("Could not initialize protocol");
        return false;
    }

    if (!NetgameManager::Connection->Connect(address, port)) {
        NETWORK_DEBUG_ERROR("Could not connect");
        return false;
    }

    NetgameManager::Started = true;
    NetgameManager::Server = false;

    return true;
}

// Connection methods
PUBLIC STATIC bool NetgameManager::Connect(const char* address, Uint16 port) {
    return NetgameManager::Connect(NETGAME_UDP, address, port);
}

PUBLIC STATIC bool NetgameManager::Connect(const char* address) {
    return NetgameManager::Connect(address, SOCKET_DEFAULT_PORT);
}

PUBLIC STATIC bool NetgameManager::Connect(Uint16 port) {
    return NetgameManager::Connect("localhost", port);
}

PUBLIC STATIC bool NetgameManager::Connect() {
    return NetgameManager::Connect(SOCKET_DEFAULT_PORT);
}

// UDP connection methods
PUBLIC STATIC bool NetgameManager::ConnectUDP(const char* address, Uint16 port) {
    return NetgameManager::Connect(address, port);
}

PUBLIC STATIC bool NetgameManager::ConnectUDP(const char* address) {
    return NetgameManager::Connect(address);
}

PUBLIC STATIC bool NetgameManager::ConnectUDP(Uint16 port) {
    return NetgameManager::Connect(port);
}

PUBLIC STATIC bool NetgameManager::ConnectUDP() {
    return NetgameManager::Connect();
}

// TCP connection methods
PUBLIC STATIC bool NetgameManager::ConnectTCP(const char* address, Uint16 port) {
    return NetgameManager::Connect(NETGAME_TCP, address, port);
}

PUBLIC STATIC bool NetgameManager::ConnectTCP(const char* address) {
    return NetgameManager::Connect(NETGAME_TCP, address, SOCKET_DEFAULT_PORT);
}

PUBLIC STATIC bool NetgameManager::ConnectTCP(Uint16 port) {
    return NetgameManager::Connect(NETGAME_TCP, "localhost", port);
}

PUBLIC STATIC bool NetgameManager::ConnectTCP() {
    return NetgameManager::Connect(NETGAME_TCP, "localhost", SOCKET_DEFAULT_PORT);
}

PUBLIC STATIC bool NetgameManager::Stop() {
    if (!NetgameManager::Started) {
        NETWORK_DEBUG_ERROR("Already stopped");
        return false;
    }

    NetgameManager::Connection->Dispose();
    NetgameManager::Started = false;
    NetgameManager::Server = false;

    return true;
}

PRIVATE STATIC bool NetgameManager::Receive() {
    for (;;) {
        ConnectionManager* conn = NetgameManager::Connection;

        conn->Receive();

        if (conn->networkClient < 0)
            return false;

        if (conn->newClient)
            NETWORK_DEBUG_INFO("New client %s", conn->GetAddress());

        NETWORK_DEBUG_INFO("Received a packet from %s", conn->GetAddress());
        NETWORK_DEBUG_INFO("Length: %d", conn->dataLength);
        NETWORK_DEBUG_INFO("Data: %.*s", conn->dataLength, conn->buffer);

        return true;
    }
}

PUBLIC STATIC void NetgameManager::Update() {
    if (!NetgameManager::Started)
        return;

    while (NetgameManager::Receive()) {
        NETWORK_DEBUG_INFO("Received a packet");
    }
}
