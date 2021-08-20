#if INTERFACE
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
};
#endif

#include <Engine/Network/ConnectionManager.h>

#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>
#include <Engine/Network/SocketManager.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/NetworkClient.h>

PUBLIC STATIC ConnectionManager* ConnectionManager::New() {
    ConnectionManager* conn = new ConnectionManager;

    conn->sockManager = new SocketManager;
    conn->init = false;
    conn->started = false;

    conn->server = false;
    conn->tcpMode = false;

    conn->networkClient = -1;
    conn->newClient = false;
    conn->dataLength = 0;

    return conn;
}

#ifdef USING_WINSOCK
static bool InitWinsock() {
    static bool started = false;
    if (started)
        return true;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        return false;
    }

    started = true;
    return true;
}
#endif

PUBLIC bool ConnectionManager::SetUDP() {
    if (init) {
        NETWORK_DEBUG_ERROR("Already initialized");
        return false;
    }

    NETWORK_DEBUG_INFO("Set UDP mode");
    tcpMode = false;
    return true;
}

PUBLIC bool ConnectionManager::SetTCP() {
    if (init) {
        NETWORK_DEBUG_ERROR("Already initialized");
        return false;
    }

    NETWORK_DEBUG_INFO("Set TCP mode");
    tcpMode = true;
    return true;
}

PUBLIC bool ConnectionManager::Init(Uint16 port) {
    if (init) {
        NETWORK_DEBUG_ERROR("Already initialized");
        return false;
    }

    NETWORK_DEBUG_INFO("Initializing");

#ifdef USING_WINSOCK
    if (!InitWinsock()) {
        NETWORK_DEBUG_ERROR("Could not start Winsock");
        return false;
    }
#endif

    if (!tcpMode) {
        NETWORK_DEBUG_INFO("Creating UDP sockets at port %d", port);

        int start, end;
        sockManager->OpenUDPSockets(port, &start, &end);

        if (start == -1 || end == -1) {
            NETWORK_DEBUG_ERROR("Could not create UDP sockets");
            return false;
        }

        NETWORK_DEBUG_INFO("Sockets created: %d", end - start);

        for (int i = start; i < end; i++)
            sockets.push_back(i);
    }

    init = true;

    return true;
}

PUBLIC bool ConnectionManager::StartUDP(Uint16 port) {
    if (started)
        return false;

    NETWORK_DEBUG_INFO("Starting in UDP mode");

    tcpMode = false;

    if (!Init(port)) {
        NETWORK_DEBUG_ERROR("Could not start in UDP mode");
        return false;
    }

    started = true;
    return true;
}

PUBLIC bool ConnectionManager::StartTCP(Uint16 port) {
    if (started)
        return false;

    NETWORK_DEBUG_INFO("Starting in TCP mode");

    tcpMode = true;

    if (!Init(port)) {
        NETWORK_DEBUG_ERROR("Could not start in TCP mode");
        return false;
    }

    started = true;
    return true;
}

PUBLIC bool ConnectionManager::Start(Uint16 port) {
    if (tcpMode) {
        return StartTCP(port);
    }

    return StartUDP(port);
}

PUBLIC bool ConnectionManager::StartUDPServer(Uint16 port) {
    if (!started && !StartUDP(port)) {
        NETWORK_DEBUG_ERROR("Could not start UDP server");
        return false;
    }

    server = true;
    return true;
}

PUBLIC bool ConnectionManager::StartTCPServer(Uint16 port) {
    if (!started && !StartTCP(port)) {
        goto fail;
    }

    int sock = sockManager->OpenTCPServer(port);
    if (sock == -1) {
        goto fail;
    }

    sockets.push_back(sock);

    server = true;
    return true;

fail:
    NETWORK_DEBUG_ERROR("Could not start TCP server");
    return false;
}

PUBLIC bool ConnectionManager::StartServer(Uint16 port) {
    if (tcpMode) {
        return StartTCPServer(port);
    }

    return StartUDPServer(port);
}

PUBLIC bool ConnectionManager::Connect(const char* address, Uint16 port) {
    if (!started && !Start(port)) {
        goto fail;
    }

    if (tcpMode) {
        int sock = sockManager->OpenTCPClient(address, port);
        if (sock == -1) {
            goto fail;
        }

        sockets.push_back(sock);
        started = true;
    } else {
        sockManager->Connect(sockets[0], address, port);
    }

    server = false;
    return true;

fail:
    if (tcpMode) {
        NETWORK_DEBUG_ERROR("Could not start in TCP mode");
    } else {
        NETWORK_DEBUG_ERROR("Could not start in UDP mode");
    }

    return false;
}

PUBLIC int ConnectionManager::NumClients() {
    return clients.size();
}

PUBLIC int ConnectionManager::CreateClient() {
    NetworkClient *client = new NetworkClient;
    client->socket = -1;

    for (int i = 0; i < NumClients(); i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            return i;
        }
    }

    clients.push_back(client);
    return clients.size() - 1;
}

PUBLIC void ConnectionManager::RemoveClient(int clientNum) {
    delete clients[clientNum];
    clients[clientNum] = NULL;
}

PUBLIC int ConnectionManager::ReceiveUDP() {
    int numSockets = sockets.size();

    for (int sock = 0; sock < numSockets; sock++) {
        SocketAddress sockAddress;

        dataLength = sockManager->Receive(sock, (Uint8*)(&buffer), (size_t)SOCKET_BUFFER_LENGTH, &sockAddress);
        if (dataLength == SOCKET_ERROR || dataLength == SOCKET_CLOSED) {
            dataLength = 0;
            continue;
        }

        NetworkClient* client = NULL;
        int numClients = NumClients();

        for (int cl = 0; cl < numClients; cl++) {
            client = clients[cl];
            if (Socket::CompareAddresses(&sockAddress, &client->address)) {
                newClient = false;
                networkClient = cl;
                client->socket = sock;
                return networkClient;
            }
        }

        newClient = true;
        networkClient = CreateClient();

        client = clients[networkClient];
        client->socket = sock;
        memcpy(&client->address, &sockAddress, sizeof(sockAddress));

        return networkClient;
    }

    networkClient = -1;
    newClient = false;

    return networkClient;
}

PUBLIC int ConnectionManager::ReceiveTCP() {
    NetworkClient* client;
    int numSockets = sockets.size();

    for (int sock = 0; sock < numSockets; sock++) {
        int newSocket = sockManager->Accept(sock);
        if (newSocket == -1) {
            continue;
        }

        NETWORK_DEBUG_INFO("Accepted new client");

        newClient = true;
        networkClient = CreateClient();

        client = clients[networkClient];
        memcpy(&client->address, sockManager->GetAddress(newSocket), sizeof(SocketAddress));
        client->socket = newSocket;
    }

    int numClients = NumClients();

    for (networkClient = 0; networkClient < numClients; networkClient++) {
        client = clients[networkClient];
        dataLength = sockManager->Receive(client->socket, (Uint8*)(&buffer), (size_t)SOCKET_BUFFER_LENGTH);

        if (dataLength == SOCKET_CLOSED) {
            sockManager->CloseSocket(client->socket);
            RemoveClient(networkClient);

            NETWORK_DEBUG_INFO("Client disconnected");

            dataLength = 0;
            continue;
        }
        else if (dataLength == SOCKET_ERROR) {
            dataLength = 0;
            continue;
        }

        return networkClient;
    }

    networkClient = -1;
    newClient = false;

    return networkClient;
}

PUBLIC int ConnectionManager::Receive() {
    if (tcpMode) {
        return ConnectionManager::ReceiveTCP();
    }

    return ConnectionManager::ReceiveUDP();
}

PUBLIC bool ConnectionManager::Send(int clientNum) {
    NetworkClient* client = clients[clientNum];

    if (sockManager->Send(client->socket, (Uint8*)(&buffer), dataLength, &client->address) == SOCKET_ERROR) {
        int error = sockManager->GetError(client->socket);
        if (error != SOCKET_EWOULDBLOCK && error != SOCKET_ECONNREFUSED) {
            return false;
        }
    }

    return true;
}

PUBLIC bool ConnectionManager::Send() {
    return ConnectionManager::Send(networkClient);
}

PUBLIC char* ConnectionManager::GetAddress(int clientNum) {
    NetworkClient* client = clients[clientNum];
    return Socket::AddressToString(sockManager->GetIPProtocol(client->socket), &client->address);
}

PUBLIC char* ConnectionManager::GetAddress() {
    return ConnectionManager::GetAddress(networkClient);
}

PUBLIC void ConnectionManager::Dispose() {
    sockManager->Dispose();
    delete this;
}
