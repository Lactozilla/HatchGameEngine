#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Connection.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/SocketManager.h>
#include <Engine/Network/Socket/Includes.h>

class ConnectionManager {
public:
    SocketManager* sockManager;
    NetworkConnection* connections[MAX_NETWORK_CONNECTIONS];
    int numConnections;

    vector<SocketAddress> loopbackAddresses;

    NetworkConnection* ownConnection;
    vector<MessageStorage> selfService;

    vector<int> sockets;
    Uint16 sockPort;

    bool init, started, connected;
    bool server, tcpMode;

    int connectionID;
    int serverID;
    bool newConnection;

    MessageStorage inBuffer;
    MessageStorage outBuffer;

    float packetDropPercentage;
};
#endif

#include <Engine/Network/Connection.h>
#include <Engine/Network/ConnectionManager.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>
#include <Engine/Network/SocketManager.h>
#include <Engine/Network/Socket/Includes.h>

#include <Engine/Diagnostics/Clock.h>
#include <Engine/Math/Math.h>

#ifdef NETWORK_DROP_PACKETS
    #define DO_PACKET_DROP \
        if (ConnectionManager::DropMessage()) \
            goto noMessage;
#else
    #define DO_PACKET_DROP
#endif

PUBLIC STATIC ConnectionManager* ConnectionManager::New() {
    ConnectionManager* conn = new ConnectionManager;

    conn->sockManager = SocketManager::New();
    conn->init = false;
    conn->started = false;
    conn->connected = false;

    conn->server = false;
    conn->tcpMode = false;

    conn->connectionID = CONNECTION_ID_NOBODY;
    conn->serverID = 0;
    conn->newConnection = false;
    conn->packetDropPercentage = 0.0;

    INIT_MESSAGE_STORAGE(conn->inBuffer);
    INIT_MESSAGE_STORAGE(conn->outBuffer);

    memset(conn->connections, 0x00, sizeof(conn->connections));
    conn->numConnections = 0;

    conn->ownConnection = new NetworkConnection;
    conn->InitConnection(conn->ownConnection);

    conn->sockets.clear();

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

    NETWORK_DEBUG_VERBOSE("Set UDP mode");
    tcpMode = false;
    return true;
}

PUBLIC bool ConnectionManager::SetTCP() {
    if (init) {
        NETWORK_DEBUG_ERROR("Already initialized");
        return false;
    }

    NETWORK_DEBUG_VERBOSE("Set TCP mode");
    tcpMode = true;
    return true;
}

PRIVATE void ConnectionManager::InitLoopback() {
    sockaddr_in ipv4;
    ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ipv4.sin_port = htons(sockPort);

    SocketAddress loopbackIPv4;
    if (Socket::SockAddrToAddress(&loopbackIPv4, AF_INET, (sockaddr_storage*)(&ipv4)) >= 0)
        loopbackAddresses.push_back(loopbackIPv4);

    sockaddr_in6 ipv6;
    ipv6.sin6_addr = in6addr_loopback;
    ipv6.sin6_port = htons(sockPort);

    SocketAddress loopbackIPv6;
    if (Socket::SockAddrToAddress(&loopbackIPv6, AF_INET6, (sockaddr_storage*)(&ipv6)) >= 0)
        loopbackAddresses.push_back(loopbackIPv6);
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
        NETWORK_DEBUG_VERBOSE("Creating UDP sockets at port %d", port);

        int start, end;
        sockManager->OpenMultipleUDPSockets(port, &start, &end);

        if (start == -1 || end == -1) {
            NETWORK_DEBUG_ERROR("Could not create UDP sockets");
            return false;
        }

        NETWORK_DEBUG_VERBOSE("Sockets created: %d", end - start);

        for (int i = start; i < end; i++)
            sockets.push_back(i);
    }

    sockPort = port;
    InitLoopback();

    init = true;

    return true;
}

PUBLIC bool ConnectionManager::StartUDP(Uint16 port) {
    if (started)
        return false;

    NETWORK_DEBUG_VERBOSE("Starting in UDP mode");

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

    NETWORK_DEBUG_VERBOSE("Starting in TCP mode");

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

    server = connected = true;
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

    server = connected = true;
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

PUBLIC int ConnectionManager::GetSocketStatus(int* activeSockets, bool reconnect) {
    int connSocket = -1;
    int i = 0;

    *activeSockets = 0;

    for (; i < (int)sockets.size();) {
        int sockNum = sockets[i];
        int connStatus = sockManager->GetConnectionStatus(sockNum);

        if (connStatus == CONNSTATUS_CONNECTED) {
            connSocket = i;
            break;
        }
        else if (connStatus == CONNSTATUS_FAILED) {
            sockManager->CloseSocket(sockNum);
            sockets.erase(sockets.begin() + i);
        }
        else {
            if (reconnect)
                sockManager->Reconnect(sockNum);
            (*activeSockets)++;
            i++;
        }
    }

    return connSocket;
}

PUBLIC void ConnectionManager::RemoveSocketsExcept(int connSocket) {
    int keepSock = sockets[connSocket];
    int numSockets = sockets.size();

    for (int i = 0; i < numSockets; i++) {
        int sockNum = sockets[i];
        if (sockNum != keepSock)
            sockManager->CloseSocket(sockNum);
    }

    sockets.clear();
    sockets.push_back(keepSock);
}

PUBLIC bool ConnectionManager::Connect(const char* address, Uint16 port) {
    if (!started && !Start(0)) {
        goto fail;
    }

    server = false;

    if (tcpMode) {
        int newSock = sockManager->AttemptToOpenTCPClient(address, port);
        if (newSock == -1) {
            NETWORK_DEBUG_ERROR("Could not create TCP socket");
            goto fail;
        }

        sockets.push_back(newSock);

        newConnection = true;
        connectionID = CreateConnection();
        serverID = connectionID;

        NetworkConnection* conn = connections[connectionID];
        memcpy(&conn->address, sockManager->GetAddress(newSock), sizeof(SocketAddress));

        conn->socket = newSock;
        conn->lastTimeReceived = Clock::GetTicks();

        NETWORK_DEBUG_IMPORTANT("Established new connection to %s", GetAddress());

        started = true;
        connected = true;

        return true;
    }
    else {
        for (int i = 0; i < (int)sockets.size(); i++)
            sockManager->Connect(sockets[i], address, port);

        connected = false;

        int activeSockets;
        int connSocket = ConnectionManager::GetSocketStatus(&activeSockets, false);

        if (connSocket >= 0) {
            connected = true;
            RemoveSocketsExcept(connSocket);
            return true;
        }

        return (activeSockets > 0) ? true : false;
    }

fail:
    if (tcpMode) {
        NETWORK_DEBUG_ERROR("Could not start in TCP mode");
    }
    else {
        NETWORK_DEBUG_ERROR("Could not start in UDP mode");
    }

    return false;
}

PUBLIC bool ConnectionManager::Reconnect() {
    if (!started)
        return false;

    int activeSockets;
    int connSocket = ConnectionManager::GetSocketStatus(&activeSockets, true);

    if (connSocket >= 0) {
        connected = true;
        RemoveSocketsExcept(connSocket);
    }
    else if (activeSockets == 0)
        return false;

    return true;
}

PUBLIC bool ConnectionManager::SetConnectionMessage(Uint8* message, size_t messageLength) {
    if (tcpMode)
        return false;

    int numSockets = sockets.size();
    for (int i = 0; i < numSockets; i++) {
        sockManager->SetConnectionMessage(sockets[i], message, messageLength);
    }

    return true;
}

PRIVATE void ConnectionManager::InitConnection(NetworkConnection* conn) {
    conn->socket = INVALID_SOCKET;
    conn->lastTimeReceived = Clock::GetTicks();

    conn->ack.sequence = 1;
    conn->ack.lastReceived = conn->ack.lastSeqAcknowledged = 0;
    conn->ack.queue.head = conn->ack.queue.tail = 0;
    conn->ack.lastTimeSent = 0.0;
}

PRIVATE int ConnectionManager::CreateConnection() {
    if (numConnections == MAX_NETWORK_CONNECTIONS)
        return CONNECTION_ID_NOBODY;

    NetworkConnection* conn = new NetworkConnection;
    InitConnection(conn);

    int i = 0;

    for (; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (connections[i] == NULL) {
            break;
        }
    }

    NETWORK_DEBUG_VERBOSE("Creating connection %d", i);

    numConnections++;
    connections[i] = conn;
    return i;
}

PRIVATE void ConnectionManager::RemoveConnection(int connectionNum) {
    if (!numConnections || connections[connectionNum] == NULL)
        return;

    delete connections[connectionNum];
    connections[connectionNum] = NULL;
    numConnections--;
}

PUBLIC void ConnectionManager::CloseConnection(int connectionNum) {
    if (!connections[connectionNum])
        return;

    int sockNum = connections[connectionNum]->socket;

    NETWORK_DEBUG_VERBOSE("Closing connection %d, socket %d", connectionNum, sockNum);

    // UDP sockets should not be closed because they don't have handles of
    // their own, unlike TCP sockets.
    if (tcpMode)
        sockManager->CloseSocket(sockNum);

    RemoveConnection(connectionNum);
}

PRIVATE bool ConnectionManager::CompareLoopback(SocketAddress* sockAddress) {
    int numAddresses = loopbackAddresses.size();

    for (int i = 0; i < numAddresses; i++) {
        if (Socket::CompareAddresses(sockAddress, &loopbackAddresses[i])) {
            return true;
        }
    }

    return false;
}

#ifdef NETWORK_DROP_PACKETS
PUBLIC void ConnectionManager::SetPacketDropPercentage(float percentage) {
    packetDropPercentage = percentage;
}

PRIVATE bool ConnectionManager::DropMessage() {
    if (packetDropPercentage == 0.0)
        return false;

    float drop = Math::Random() * 100.0;
    return (drop < packetDropPercentage);
}
#endif

PRIVATE bool ConnectionManager::ReceiveOwnMessages() {
    if (selfService.size()) {
        MessageStorage message = selfService.back();

        inBuffer.length = message.length;
        memcpy(inBuffer.data, message.data, inBuffer.length);

        connectionID = CONNECTION_ID_SELF;
        newConnection = false;
        selfService.pop_back();

        return true;
    }

    return false;
}

PRIVATE int ConnectionManager::ReceiveUDP() {
    if (ReceiveOwnMessages()) {
        DO_PACKET_DROP;
        return connectionID;
    }

    int numSockets = sockets.size();

    for (int sock = 0; sock < numSockets; sock++) {
        SocketAddress sockAddress;

        inBuffer.length = sockManager->Receive(sockets[sock], (Uint8*)(&inBuffer.data), (size_t)SOCKET_BUFFER_LENGTH, &sockAddress);
        if (inBuffer.length == SOCKET_ERROR || inBuffer.length == SOCKET_CLOSED) {
            inBuffer.length = 0;
            continue;
        }

        // Ignore messages from loopback addresses
        if (CompareLoopback(&sockAddress)) {
            continue;
        }

        DO_PACKET_DROP;

        NetworkConnection* conn = NULL;

        for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
            conn = connections[i];
            if (!conn)
                continue;

            if (Socket::CompareAddresses(&sockAddress, &conn->address)) {
                newConnection = false;
                connectionID = i;
                conn->socket = sock;
                conn->lastTimeReceived = Clock::GetTicks();
                return connectionID;
            }
        }

        connectionID = CreateConnection();
        if (connectionID < 0) {
            newConnection = false;
            return connectionID;
        }

        newConnection = true;
        sockManager->SetConnected(sock);

        conn = connections[connectionID];
        conn->socket = sock;
        conn->lastTimeReceived = Clock::GetTicks();
        memcpy(&conn->address, &sockAddress, sizeof(sockAddress));

        NETWORK_DEBUG_IMPORTANT("New connection from %s", GetAddress());

        return connectionID;
    }

#ifdef NETWORK_DROP_PACKETS
noMessage:
#endif

    connectionID = CONNECTION_ID_NOBODY;
    newConnection = false;

    return connectionID;
}

PRIVATE bool ConnectionManager::Accept() {
    int numSockets = sockets.size();

    for (int sock = 0; sock < numSockets; sock++) {
        int newSocket = sockManager->Accept(sockets[sock]);
        if (newSocket == -1) {
            continue;
        }

        newConnection = true;
        connectionID = CreateConnection();

        NetworkConnection* conn = connections[connectionID];
        memcpy(&conn->address, sockManager->GetAddress(newSocket), sizeof(SocketAddress));

        conn->socket = newSocket;
        conn->lastTimeReceived = Clock::GetTicks();

        NETWORK_DEBUG_IMPORTANT("Accepted new connection from %s", GetAddress());

        if (ReceiveFromTCPConnection())
            return true;
    }

    return false;
}

PRIVATE int ConnectionManager::ReceiveTCP() {
    if (ReceiveOwnMessages())
        return connectionID;

    if (server && Accept())
        return connectionID;

    newConnection = false;

    for (connectionID = 0; connectionID < MAX_NETWORK_CONNECTIONS; connectionID++) {
        if (ReceiveFromTCPConnection())
            return connectionID;
    }

    connectionID = CONNECTION_ID_NOBODY;

    return connectionID;
}

PRIVATE bool ConnectionManager::ReceiveFromTCPConnection() {
    NetworkConnection* conn = connections[connectionID];
    if (!conn)
        return false;

    inBuffer.length = sockManager->Receive(conn->socket, (Uint8*)(&inBuffer.data), (size_t)SOCKET_BUFFER_LENGTH);

    if (inBuffer.length == SOCKET_CLOSED) {
        CloseConnection(connectionID);

        NETWORK_DEBUG_VERBOSE("Connection %d disconnected", connectionID);

        inBuffer.length = 0;
        return false;
    }
    else if (inBuffer.length == SOCKET_ERROR) {
        inBuffer.length = 0;
        return false;
    }

    conn->lastTimeReceived = Clock::GetTicks();

    return true;
}

PRIVATE void ConnectionManager::SendToSelf() {
    MessageStorage message;

    memcpy(message.data, outBuffer.data, outBuffer.length);
    message.length = outBuffer.length;

    selfService.insert(selfService.begin(), message);
}

PUBLIC bool ConnectionManager::Send(int connectionNum) {
    if (connectionNum == CONNECTION_ID_SELF) {
        SendToSelf();
        return true;
    }
    else if (!CONNECTION_ID_VALID(connectionNum))
        return false;

    NetworkConnection* conn = connections[connectionNum];
    if (!conn)
        return false;

    if (sockManager->Send(conn->socket, (Uint8*)(&outBuffer.data), outBuffer.length, &conn->address) == SOCKET_ERROR) {
        int error = sockManager->GetError(conn->socket);
        if (error != SOCKET_EWOULDBLOCK) {
            NETWORK_DEBUG_ERROR("Socket error in connection %d: %s", connectionNum, Socket::GetErrorString(error));
            ConnectionManager::CloseConnection(connectionNum);
            return false;
        }
    }

    return true;
}

PUBLIC bool ConnectionManager::Send(int connectionNum, size_t length) {
    outBuffer.length = length;
    return ConnectionManager::Send(connectionNum);
}

PUBLIC bool ConnectionManager::Send() {
    return ConnectionManager::Send(connectionID);
}

PUBLIC void ConnectionManager::WriteOutgoingData(Uint8* data, size_t length) {
    memcpy(outBuffer.data, data, length);
    outBuffer.length = length;
}

PUBLIC Uint8* ConnectionManager::ReadOutgoingData(size_t* length) {
    *length = (size_t)outBuffer.length;
    return outBuffer.data;
}

PUBLIC Uint8* ConnectionManager::ReadIncomingData(size_t* length) {
    *length = (size_t)inBuffer.length;
    return inBuffer.data;
}

PUBLIC int ConnectionManager::Receive() {
    if (tcpMode) {
        return ConnectionManager::ReceiveTCP();
    }

    return ConnectionManager::ReceiveUDP();
}

PUBLIC char* ConnectionManager::GetAddress(int connectionNum) {
    if (connectionNum == CONNECTION_ID_SELF)
        return "(myself)";
    else if (!CONNECTION_ID_VALID(connectionNum))
        return "(nobody)";

    NetworkConnection* conn = connections[connectionNum];
    return Socket::AddressToString(sockManager->GetIPProtocol(conn->socket), &conn->address);
}

PUBLIC char* ConnectionManager::GetAddress() {
    return ConnectionManager::GetAddress(connectionID);
}

PUBLIC void ConnectionManager::Update() {
    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        NetworkConnection* conn = connections[i];
        if (!conn)
            continue;

        double time = Clock::GetTicks() - conn->lastTimeReceived;

        if (time >= NETWORK_SOCKET_TIMEOUT * 1000.0) {
            NETWORK_DEBUG_IMPORTANT("Connection %d took too long to respond", i);
            ConnectionManager::CloseConnection(i);
        }
    }
}

PUBLIC void ConnectionManager::Dispose() {
    sockManager->Dispose();
    delete this;
}
