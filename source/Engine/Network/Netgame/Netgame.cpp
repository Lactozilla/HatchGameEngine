#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>
#include <Engine/Network/ConnectionManager.h>

class Netgame {
public:
    static ConnectionManager* Connection;
    static bool Started, UseAcks;
    static int Protocol;

    static char ClientName[MAX_CLIENT_NAME];

    static bool Server;
    static bool SocketReady;
    static int Connected;

    static double LastHeartbeat;
    static double ReconnectionTimer;

    static int PlayerCount, PlayerID;
    static NetworkPlayer Players[MAX_NETGAME_PLAYERS];

    static Packet PacketsToResend[MAX_MESSAGES_TO_RESEND];

    static NetworkCommands PlayerCommands;
    static Uint32 CurrentFrame, NextFrame;
};
#endif

#include <Engine/Network/Network.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>
#include <Engine/Network/ConnectionManager.h>
#include <Engine/Network/Ack.h>

#include <Engine/Network/Netgame/Netgame.h>
#include <Engine/Network/Netgame/NetgameClient.h>
#include <Engine/Network/Netgame/NetgameServer.h>

#include <Engine/Types/EntityTypes.h>
#include <Engine/Types/ObjectList.h>

#include <Engine/Diagnostics/Clock.h>

ConnectionManager* Netgame::Connection = NULL;
bool               Netgame::Started = false;
bool               Netgame::UseAcks = true;
int                Netgame::Protocol = PROTOCOL_UDP;

char               Netgame::ClientName[MAX_CLIENT_NAME];

bool               Netgame::Server = false;
bool               Netgame::SocketReady = false;
int                Netgame::Connected = CONNSTATUS_DISCONNECTED;

double             Netgame::LastHeartbeat = 0.0;
double             Netgame::ReconnectionTimer = 0.0;

int                Netgame::PlayerCount = 0;
int                Netgame::PlayerID = 0;
NetworkPlayer      Netgame::Players[MAX_NETGAME_PLAYERS];

Packet             Netgame::PacketsToResend[MAX_MESSAGES_TO_RESEND];

NetworkCommands    Netgame::PlayerCommands;
Uint32             Netgame::CurrentFrame = 0;
Uint32             Netgame::NextFrame = 0;

#define IS_CLIENT (!Server)

PUBLIC STATIC bool Netgame::Start() {
    if (Netgame::Started) {
        NETWORK_DEBUG_ERROR("Already started");
        return false;
    }

    if (!Netgame::CreateConnManager())
        return false;

    Netgame::Cleanup();

    Netgame::Started = true;

    return true;
}

PUBLIC STATIC void Netgame::Disconnect() {
    if (Netgame::IsDisconnected()) {
        NETWORK_DEBUG_ERROR("Already disconnected");
        return;
    }

    NETWORK_DEBUG_IMPORTANT("Disconnecting");

    if (IS_CLIENT)
        NetgameClient::Disconnect();
    else
        NetgameServer::Disconnect();
}

PUBLIC STATIC void Netgame::FatalError() {
    Netgame::Stop();
    exit(-1);
}

PUBLIC STATIC bool Netgame::Stop() {
    if (!Netgame::Started) {
        NETWORK_DEBUG_ERROR("Already stopped");
        return false;
    }

    NETWORK_DEBUG_IMPORTANT("Stopping");

    if (!Netgame::IsDisconnected())
        Netgame::Disconnect();

    Netgame::Connection->Dispose();
    Netgame::Started = false;
    Netgame::Connected = CONNSTATUS_DISCONNECTED;
    Netgame::Cleanup();

    memset(Netgame::ClientName, 0x00, MAX_CLIENT_NAME);

    return true;
}

PUBLIC STATIC void Netgame::Cleanup() {
    Netgame::LastHeartbeat = 0.0;
    Netgame::ReconnectionTimer = 0.0;

    Netgame::Server = false;
    Netgame::SetServerID(CONNECTION_ID_NOBODY);

    Netgame::PlayerCount = 0;
    Netgame::PlayerID = 0;

    memset(Netgame::Players, 0x00, sizeof(Netgame::Players));

    Netgame::ClearInputs(&Netgame::PlayerCommands);

    NetgameServer::Cleanup();
    NetgameClient::Cleanup();

    Netgame::CurrentFrame = Netgame::NextFrame = 0;

    // Oh! Fuhgeddaboudit!!!
    Packet::ResendListClear();
}

PRIVATE STATIC bool Netgame::Restart() {
    if (Netgame::Started)
        Netgame::Stop();

    return Netgame::Start();
}

PRIVATE STATIC bool Netgame::CreateConnManager() {
    Netgame::Connection = ConnectionManager::New();
    if (Netgame::Connection == NULL) {
        NETWORK_DEBUG_ERROR("Could not create ConnectionManager");
        return false;
    }

    return true;
}

PRIVATE STATIC bool Netgame::InitProtocol(int protocol) {
    if (Netgame::Connection == NULL) {
        NETWORK_DEBUG_ERROR("No ConnectionManager");
        return false;
    }

    if (protocol == PROTOCOL_UDP) {
        NETWORK_DEBUG_VERBOSE("UDP mode");

        if (!Netgame::Connection->SetUDP()) {
            NETWORK_DEBUG_ERROR("Could not set UDP mode");
            return false;
        }

        Netgame::UseAcks = true;
    }
    else if (protocol == PROTOCOL_TCP) {
        NETWORK_DEBUG_VERBOSE("TCP mode");

        if (!Netgame::Connection->SetTCP()) {
            NETWORK_DEBUG_ERROR("Could not set TCP mode");
            return false;
        }

        Netgame::UseAcks = false;
    }
    else {
        NETWORK_DEBUG_ERROR("Unknown protocol set");
        return false;
    }

    Netgame::Protocol = protocol;

    return true;
}

PUBLIC STATIC bool Netgame::StartServer(int protocol, Uint16 port) {
    bool alreadyStarted = Netgame::Started;

    if (!alreadyStarted && !Netgame::Start()) {
        NETWORK_DEBUG_ERROR("Could not start");
        return false;
    }

    if (alreadyStarted) {
        NETWORK_DEBUG_IMPORTANT("Restarting server");
        Netgame::Restart();
    }
    else
        NETWORK_DEBUG_IMPORTANT("Starting server");

    if (!Netgame::InitProtocol(protocol)) {
        NETWORK_DEBUG_ERROR("Could not initialize protocol");
        return false;
    }

    if (!Netgame::Connection->StartServer(port)) {
        NETWORK_DEBUG_ERROR("Could not start server");
        return false;
    }

    Netgame::Server = true;
    Netgame::SetServerID(CONNECTION_ID_SELF);

    if (!Netgame::ClientName[0])
        strncpy(Netgame::ClientName, "Server", MAX_CLIENT_NAME);

    Netgame::Connected = CONNSTATUS_CONNECTED;
    Netgame::UpdateHeartbeat();

    NetgameServer::MaxPlayerCount = MAX_NETGAME_PLAYERS;
    NetgameServer::JoinsAllowed = true;
    NetgameServer::AddOwnPlayer();

#ifdef NETWORK_DROP_PACKETS
    Netgame::Connection->SetPacketDropPercentage(NETWORK_PACKET_DROP_PERCENTAGE);
#endif

    return true;
}

// Server methods
PUBLIC STATIC bool Netgame::StartServer(Uint16 port) {
    return Netgame::StartServer(PROTOCOL_UDP, port);
}

PUBLIC STATIC bool Netgame::StartServer() {
    return Netgame::StartServer(PROTOCOL_UDP, SOCKET_DEFAULT_PORT);
}

// UDP server methods
PUBLIC STATIC bool Netgame::StartUDPServer(Uint16 port) {
    return Netgame::StartServer(port);
}

PUBLIC STATIC bool Netgame::StartUDPServer() {
    return Netgame::StartServer();
}

// TCP server methods
PUBLIC STATIC bool Netgame::StartTCPServer(Uint16 port) {
    return Netgame::StartServer(PROTOCOL_TCP, port);
}

PUBLIC STATIC bool Netgame::StartTCPServer() {
    return Netgame::StartServer(PROTOCOL_TCP, SOCKET_DEFAULT_PORT);
}

// Connection
PUBLIC STATIC bool Netgame::IsDisconnected() {
    return (Netgame::Connected == CONNSTATUS_DISCONNECTED || Netgame::Connected == CONNSTATUS_FAILED);
}

PUBLIC STATIC bool Netgame::IsAttemptingConnection() {
    return (Netgame::Connected == CONNSTATUS_ATTEMPTING);
}

PUBLIC STATIC bool Netgame::IsConnected() {
    return (Netgame::Connected == CONNSTATUS_CONNECTED);
}

PUBLIC STATIC bool Netgame::DidConnectionFail() {
    return (Netgame::Connected == CONNSTATUS_FAILED);
}

PRIVATE STATIC bool Netgame::SendConnectionRequest() {
    if (Netgame::Server)
        return false;

    if (!Netgame::IsAttemptingConnection())
        return false;

    if (NetgameClient::WasAccepted())
        NetgameClient::SendReady();
    else
        NetgameClient::SendJoin();

    return true;
}

PRIVATE STATIC void Netgame::PrepareJoinMessage() {
    NetgameClient::PrepareJoin();

    size_t joinMessageLength;
    Uint8* joinMessage = Netgame::Connection->ReadOutgoingData(&joinMessageLength);

    Netgame::Connection->SetConnectionMessage(joinMessage, joinMessageLength);
}

PUBLIC STATIC bool Netgame::Connect(int protocol, const char* address, Uint16 port) {
    bool alreadyStarted = Netgame::Started;

    if (!alreadyStarted && !Netgame::Start()) {
        NETWORK_DEBUG_ERROR("Could not start");
        return false;
    }

    ConnectionManager* conn = Netgame::Connection;

    if (alreadyStarted) {
        NETWORK_DEBUG_IMPORTANT("Reconnecting to %s at port %d", address, port);

        Netgame::Restart();

        conn = Netgame::Connection;
    }
    else
        NETWORK_DEBUG_IMPORTANT("Connecting to %s at port %d", address, port);

    if (!Netgame::InitProtocol(protocol)) {
        NETWORK_DEBUG_ERROR("Could not initialize protocol");
        return false;
    }

    Netgame::Connected = CONNSTATUS_ATTEMPTING;

    if (Netgame::Protocol == PROTOCOL_UDP) {
        if (!conn->StartUDP(0)) {
            NETWORK_DEBUG_ERROR("Could not start ConnectionManager in UDP mode");
            Netgame::Connected = CONNSTATUS_FAILED;
            return false;
        }

        Netgame::PrepareJoinMessage();
    }
    else if (!conn->StartTCP(0)) {
        NETWORK_DEBUG_ERROR("Could not start ConnectionManager in TCP mode");
        Netgame::Connected = CONNSTATUS_FAILED;
        return false;
    }

    if (!conn->Connect(address, port)) {
        NETWORK_DEBUG_ERROR("Could not connect");
        Netgame::Connected = CONNSTATUS_FAILED;
        return false;
    }

    if (Netgame::Protocol == PROTOCOL_TCP) {
        Netgame::SetServerID(conn->serverID);
    }

    Netgame::ReconnectionTimer = Clock::GetTicks();

    if (conn->connected) {
        Netgame::SocketReady = true;
        Netgame::SendConnectionRequest();
    }

    return true;
}

// Connection methods
PUBLIC STATIC bool Netgame::Connect(const char* address, Uint16 port) {
    return Netgame::Connect(PROTOCOL_UDP, address, port);
}

PUBLIC STATIC bool Netgame::Connect(const char* address) {
    return Netgame::Connect(address, SOCKET_DEFAULT_PORT);
}

PUBLIC STATIC bool Netgame::Connect(Uint16 port) {
    return Netgame::Connect("localhost", port);
}

PUBLIC STATIC bool Netgame::Connect() {
    return Netgame::Connect(SOCKET_DEFAULT_PORT);
}

// UDP connection methods
PUBLIC STATIC bool Netgame::ConnectUDP(const char* address, Uint16 port) {
    return Netgame::Connect(address, port);
}

PUBLIC STATIC bool Netgame::ConnectUDP(const char* address) {
    return Netgame::Connect(address);
}

PUBLIC STATIC bool Netgame::ConnectUDP(Uint16 port) {
    return Netgame::Connect(port);
}

PUBLIC STATIC bool Netgame::ConnectUDP() {
    return Netgame::Connect();
}

// TCP connection methods
PUBLIC STATIC bool Netgame::ConnectTCP(const char* address, Uint16 port) {
    return Netgame::Connect(PROTOCOL_TCP, address, port);
}

PUBLIC STATIC bool Netgame::ConnectTCP(const char* address) {
    return Netgame::Connect(PROTOCOL_TCP, address, SOCKET_DEFAULT_PORT);
}

PUBLIC STATIC bool Netgame::ConnectTCP(Uint16 port) {
    return Netgame::Connect(PROTOCOL_TCP, "localhost", port);
}

PUBLIC STATIC bool Netgame::ConnectTCP() {
    return Netgame::Connect(PROTOCOL_TCP, "localhost", SOCKET_DEFAULT_PORT);
}

// Receive
PRIVATE STATIC bool Netgame::Receive() {
    ConnectionManager* conn = Netgame::Connection;

    for (;;) {
        int connectionID = conn->Receive();
        if (!CONNECTION_ID_VALID(connectionID))
            return false;

        return true;
    }

    return false;
}

// Send
PUBLIC STATIC bool Netgame::Send(int connectionID) {
    if (!CONNECTION_ID_VALID(connectionID)) {
        NETWORK_DEBUG_ERROR("Invalid connection ID %d", connectionID);
        return false;
    }

    return Netgame::Connection->Send(connectionID);
}

// Outgoing and incoming data
PUBLIC STATIC void Netgame::WriteOutgoingData(Uint8* data, size_t length) {
    Netgame::Connection->WriteOutgoingData(data, length);
}

PUBLIC STATIC Uint8* Netgame::ReadOutgoingData(size_t* length) {
    return Netgame::Connection->ReadOutgoingData(length);
}

PUBLIC STATIC Uint8* Netgame::ReadIncomingData(size_t* length) {
    return Netgame::Connection->ReadIncomingData(length);
}

// Connection methods
PUBLIC STATIC void Netgame::CloseConnection(int connection, bool sendQuit) {
    if (connection == CONNECTION_ID_SELF || !CONNECTION_ID_VALID(connection))
        return;

    NETWORK_DEBUG_VERBOSE("Closing connection %d", connection);

    if (Server) {
        if (NetgameServer::ClientInGame(connection)) {
            if (sendQuit)
                NetgameServer::SendQuit(connection);

            NetgameServer::RemovePlayer(NetgameServer::GetClientPlayer(connection));
        }

        NetgameServer::ClientQuit(connection);
    }

    Packet::RemovePendingPackets(connection);

    Netgame::Connection->CloseConnection(connection);
}

PUBLIC STATIC void Netgame::CloseConnection(int connection) {
    Netgame::CloseConnection(connection, true);
}

PUBLIC STATIC NetworkConnection* Netgame::GetConnection(int connection) {
    if (connection == CONNECTION_ID_SELF)
        return Netgame::Connection->ownConnection;
    else if (!CONNECTION_ID_VALID(connection))
        return NULL;

    return Netgame::Connection->connections[connection];
}

PUBLIC STATIC NetworkConnection* Netgame::GetConnection() {
    return Netgame::GetConnection(Netgame::GetConnectionID());
}

PUBLIC STATIC int Netgame::GetConnectionID() {
    return Netgame::Connection->connectionID;
}

PUBLIC STATIC int Netgame::GetServerID() {
    return Netgame::Connection->serverID;
}

PUBLIC STATIC void Netgame::SetServerID(int serverID) {
    Netgame::Connection->serverID = serverID;
}

PUBLIC STATIC bool Netgame::NewClientConnected() {
    return Netgame::Connection->newConnection;
}

// Add and remove players
PUBLIC STATIC void Netgame::InsertPlayerIntoSlot(Uint8 playerID, char* name, int client) {
    NetworkPlayer* player = &Netgame::Players[playerID];
    player->ingame = true;

    memset(player->name, 0x00, MAX_CLIENT_NAME);
    memset(player->pendingName, 0x00, MAX_CLIENT_NAME);

    OnNameChange(playerID, name, false);
    ChangePlayerName(playerID, player->pendingName);

    NETWORK_DEBUG_IMPORTANT("Adding player %s into slot %d", player->name, playerID);

    if (Server) {
        player->client = client;

        if (client != CONNECTION_ID_SELF)
            NetgameServer::SetClientPlayer(client, playerID);
    }
    else
        player->client = CONNECTION_ID_NOBODY;
}

PUBLIC STATIC int Netgame::AddPlayer(char* name, int client) {
    int i = 0;

    for (; i < MAX_NETGAME_PLAYERS; i++) {
        if (!Netgame::Players[i].ingame) {
            break;
        }
    }

    if (i == MAX_NETGAME_PLAYERS)
        return -1;

    Netgame::PlayerCount++;
    Netgame::InsertPlayerIntoSlot(i, name, client);

    return i;
}

PUBLIC STATIC void Netgame::RemovePlayer(Uint8 playerID) {
    if (playerID < 0 || playerID >= MAX_NETGAME_PLAYERS)
        return;

    if (Netgame::PlayerCount)
        Netgame::PlayerCount--;

    NetworkPlayer* player = &Netgame::Players[playerID];

    // TODO: Scripting layer callback

    if (Server)
        NetgameServer::SetClientPlayer(player->client, CONNECTION_ID_NOBODY);

    memset(player, 0x00, sizeof(NetworkPlayer));
}

// Returns true if the specified player ID is in-game
PUBLIC STATIC bool Netgame::IsPlayerInGame(Uint8 playerID) {
    if (playerID == (Uint8)CONNECTION_ID_NOBODY)
        return false;
    return Netgame::Players[playerID].ingame;
}

// Returns the name of the specified player ID
PUBLIC STATIC char* Netgame::GetPlayerName(Uint8 playerID) {
    return Netgame::Players[playerID].name;
}

// Renames a player
PUBLIC STATIC bool Netgame::OnNameChange(Uint8 playerID, char* playerName, bool callback) {
    NetworkPlayer* player = &Netgame::Players[playerID];

    strncpy(player->pendingName, playerName, MAX_CLIENT_NAME);
    player->pendingName[MAX_CLIENT_NAME - 1] = '\0';

    (void)callback; // TODO: Scripting layer callback

    if (strlen(player->pendingName) == 0) {
        char defaultName[10]; // Player #X
        snprintf(defaultName, sizeof(defaultName), "Player #%d", playerID + 1);
        strncpy(player->pendingName, defaultName, MAX_CLIENT_NAME);
    }
    else if (!strcmp(player->name, player->pendingName))
        return false;

    return true;
}

PUBLIC STATIC void Netgame::ChangePlayerName(Uint8 playerID, char* playerName) {
    NetworkPlayer* player = &Netgame::Players[playerID];
    strncpy(player->name, playerName, MAX_CLIENT_NAME);
    player->name[MAX_CLIENT_NAME - 1] = '\0';
}

// Changes your name
PUBLIC STATIC void Netgame::ChangeName(char* playerName) {
    if (Netgame::IsAttemptingConnection() || Netgame::IsConnected()) {
        INIT_MESSAGE(message, MESSAGE_NAMECHANGE);

        message.nameChange.playerID = -1; // Doesn't matter
        strncpy(message.nameChange.name, playerName, MAX_CLIENT_NAME);

        SEND_MESSAGE(message, nameChange, Netgame::GetServerID());
        return;
    }

    strncpy(Netgame::ClientName, playerName, MAX_CLIENT_NAME);
}

// Handle incoming messages
#define CAN_CLOSE_CONNECTION(from) (IS_CLIENT || (Server && !NetgameServer::ClientInGame(from)))

#define SERVER_ONLY \
    if (IS_CLIENT) { \
        NETWORK_DEBUG_ERROR("Received a message that a client cannot handle"); \
        Netgame::CloseConnection(from); \
        return false; \
    }

#define CLIENT_ONLY \
    if (Server) { \
        NETWORK_DEBUG_ERROR("Received a message that a server cannot handle"); \
        if (from != CONNECTION_ID_SELF && !NetgameServer::ClientInGame(from)) \
            Netgame::CloseConnection(from); \
        return false; \
    }

#define CHECK_FOREIGN_MESSAGE \
    if (from != Netgame::GetServerID()) { \
        NETWORK_DEBUG_ERROR("Received a message that did not come from the server"); \
        Netgame::CloseConnection(from); \
        return false; \
    }

#define CHECK_CLIENT_IN_GAME \
    if (from != CONNECTION_ID_SELF && !NetgameServer::ClientInGame(from)) \
        return false;

PRIVATE STATIC bool Netgame::ProcessMessage(Message* message, int from) {
    if (Server) {
        NetgameServer::UpdateLastTimeContacted(from);
    }

    bool tcpMode = (Netgame::Protocol == PROTOCOL_TCP);

    if (IS_CLIENT) {
        bool checkServerResponse = tcpMode;
        if (!tcpMode && Netgame::GetServerID() == CONNECTION_ID_NOBODY)
            checkServerResponse = true;

        // 1. Client sends join message.
        // 2. Server receives join message. Server sends accept or refusal message.
        // 3. Client receives accept message. Client sends ready message.
        // 4. Server receives ready message. Client is considered ready. Server sends ready message.
        // 5. Client receives ready message. Client is connected.
        if (Netgame::IsAttemptingConnection() && checkServerResponse) {
            if (tcpMode) {
                CHECK_FOREIGN_MESSAGE
            }

            switch (message->type) {
                case MESSAGE_ACCEPT:
                    if (!tcpMode)
                        Netgame::SetServerID(from);
                    NetgameClient::ReceiveAccept(message);
                    return true;
                case MESSAGE_REFUSE:
                    Netgame::CloseConnection(from);
                    NetgameClient::ReceiveRefusal(message);
                    return true;
            }
        }

        if (!Netgame::IsAttemptingConnection()) {
            // Ignore messages from connections that don't belong to the server,
            // if the client is not attempting to connect into the server.
            CHECK_FOREIGN_MESSAGE
        }
        else if (!Netgame::SocketReady) {
            // Ignore messages from connections that don't belong to the server,
            // if it's not an accept or a refuse message, and the client is
            // attempting to connect into the server.
            CHECK_FOREIGN_MESSAGE
        }
    }

    switch (message->type) {
        // Messages handled by the server
        case MESSAGE_JOIN:
            SERVER_ONLY
            if (!NetgameServer::ClientDisconnected(from))
                break;

            if (!NetgameServer::JoinsAllowed) {
                NetgameServer::SendRefusal(from, REFUSE_NOTALLOWED);
                return true;
            }
            else if (Netgame::PlayerCount >= NetgameServer::MaxPlayerCount) {
                NetgameServer::SendRefusal(from, REFUSE_PLAYERCOUNT);
                return true;
            }

            NetgameServer::ClientWaiting(message, from);
            NetgameServer::SendAccept(from);
            return true;
        case MESSAGE_COMMANDS:
            SERVER_ONLY
            if (from != CONNECTION_ID_SELF && NetgameServer::ClientDisconnected(from))
                break;

            NetgameServer::ReadCommands(message, from);
            return true;
        // Messages handled by clients
        case MESSAGE_ACCEPT:
        case MESSAGE_REFUSE:
            CLIENT_ONLY
            if (!tcpMode && Netgame::GetServerID() != CONNECTION_ID_NOBODY)
                return false;
            else if (!Netgame::IsAttemptingConnection()) {
                CHECK_FOREIGN_MESSAGE
                return false;
            }
            return true;
        case MESSAGE_PLAYERJOIN:
            CLIENT_ONLY
            return NetgameClient::ReceivePlayerJoin(message);
        case MESSAGE_PLAYERLEAVE:
            CLIENT_ONLY
            return NetgameClient::ReceivePlayerLeave(message);
        case MESSAGE_SERVERCOMMANDS:
            CLIENT_ONLY
            NetgameClient::NetgameClient::ReadCommands(message);
            return true;
        // Messages handled by either
        case MESSAGE_READY:
            if (Server) {
                if (!NetgameServer::WaitingReadyFromClient(from)) {
                    if (NetgameServer::ClientInGame(from)) // Weren't you ready?
                        NetgameServer::ResendReady(from);
                    else
                        Netgame::CloseConnection(from);
                    return true;
                }

                NetgameServer::ClientReady(from);
                return true;
            }
            else if (Netgame::IsAttemptingConnection()) {
                NetgameClient::ReceiveReady(message);
                return true;
            }
            goto closeConnection;
        case MESSAGE_NAMECHANGE:
            if (Server) {
                CHECK_CLIENT_IN_GAME
                NetgameServer::BroadcastNameChange(message, from);
            }
            else {
                NetgameClient::ReceiveNameChange(message);
            }
            return true;
        case MESSAGE_ACKNOWLEDGEMENT:
            if (!Netgame::UseAcks)
                break;
            Ack::Acknowledge(message, from);
            return true;
        case MESSAGE_HEARTBEAT:
            return true;
        case MESSAGE_QUIT:
            if (Server)
                Netgame::CloseConnection(from);
            else
                NetgameClient::Disconnect();
            return true;
        default:
        closeConnection:
            if (CAN_CLOSE_CONNECTION(from))
                Netgame::CloseConnection(from);
            break;
    }

    return false;
}

#undef CAN_CLOSE_CONNECTION
#undef SERVER_ONLY
#undef CLIENT_ONLY

// Update methods
PRIVATE STATIC bool Netgame::CheckConnections() {
    NetworkConnection* conn;

    if (Server) {
        for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
            conn = Netgame::Connection->connections[i];

            if (!conn && !NetgameServer::ClientDisconnected(i)) {
                NETWORK_DEBUG_IMPORTANT("Client %d is no longer connected", i);
                Netgame::CloseConnection(i, false);
            }
        }
    }
    else {
        int serverID = Netgame::GetServerID();
        if (serverID == CONNECTION_ID_SELF || !CONNECTION_ID_VALID(serverID))
            return true;

        conn = Netgame::Connection->connections[serverID];
        if (!conn) {
            NETWORK_DEBUG_IMPORTANT("No longer connected to server");
            NetgameClient::Disconnect();
            return false;
        }
    }

    return true;
}

PRIVATE STATIC bool Netgame::AttemptConnection() {
    double time = Clock::GetTicks() - Netgame::ReconnectionTimer;

    if (time >= CLIENT_RECONNECTION_INTERVAL) {
        Netgame::ReconnectionTimer = Clock::GetTicks();

        if (Netgame::SocketReady) {
            Netgame::SendConnectionRequest();
        }
        else if (Netgame::Protocol == PROTOCOL_UDP && !Netgame::Connection->Reconnect()) {
            Netgame::Connected = CONNSTATUS_FAILED;
            return false;
        }

        if (Netgame::Connection->connected)
            Netgame::SocketReady = true;
    }

    return true;
}

PUBLIC STATIC void Netgame::Heartbeat() {
    double interval = Clock::GetTicks() - Netgame::LastHeartbeat;

    if (interval >= NETWORK_HEARTBEAT_INTERVAL) {
        if (IS_CLIENT)
            NetgameClient::Heartbeat();
        else
            NetgameServer::Heartbeat();

        Netgame::UpdateHeartbeat();
    }
}

PUBLIC STATIC void Netgame::UpdateHeartbeat() {
    Netgame::LastHeartbeat = Clock::GetTicks();
}

PUBLIC STATIC void Netgame::GetMessages() {
    while (Netgame::Receive()) {
        Message message;

        if (Packet::Process(&message))
            Netgame::ProcessMessage(&message, Netgame::GetConnectionID());
    }
}

static const char *NetworkCallbackRegistry = "NetworkCallback";

PUBLIC STATIC void Netgame::Callback_SendCommands() {
    ObjectList* objectList;
    if (!Scene::ObjectRegistries->Exists(NetworkCallbackRegistry))
        return;

    objectList = Scene::ObjectRegistries->Get(NetworkCallbackRegistry);

    int objectListCount = objectList->Count();
    for (int o = 0; o < objectListCount; o++) {
        Entity* ent = objectList->GetNth(o);
        if (ent && ent->Active && ent->Interactable) {
            ent->Network_SendCommands(&Netgame::PlayerCommands);
        }
    }
}

PUBLIC STATIC void Netgame::Callback_ReceiveCommands(Uint32 frame) {
    ObjectList* objectList;
    if (!Scene::ObjectRegistries->Exists(NetworkCallbackRegistry))
        return;

    objectList = Scene::ObjectRegistries->Get(NetworkCallbackRegistry);

    int objectListCount = objectList->Count();
    for (int o = 0; o < objectListCount; o++) {
        Entity* ent = objectList->GetNth(o);
        if (!ent || !ent->Active || !ent->Interactable) {
            continue;
        }

        for (Uint8 playerID = 0; playerID < MAX_NETGAME_PLAYERS; playerID++) {
            NetworkPlayer* player = &Netgame::Players[playerID];
            if (!player->ingame) {
                continue;
            }

            Uint32 playerFrame = frame % INPUT_BUFFER_FRAMES;
            if (player->receivedCommands[playerFrame])
                ent->Network_ReceiveCommands(playerID, &player->commands[playerFrame]);
            player->receivedCommands[playerFrame] = false;
        }
    }
}

PUBLIC STATIC void Netgame::ClearInputs(NetworkCommands* commands) {
    memset(commands, 0x00, sizeof(NetworkCommands));
}

PUBLIC STATIC void Netgame::CopyInputs(NetworkCommands* dest, NetworkCommands* source) {
    memcpy(dest, source, sizeof(NetworkCommands));
}

PUBLIC STATIC void Netgame::GetInputs() {
    Netgame::Callback_SendCommands();
}

PUBLIC STATIC void Netgame::SendInputs() {
    INIT_MESSAGE(message, MESSAGE_COMMANDS);

    message.clientCommands.frame = Packet::SwapLong(Netgame::NextFrame);
    message.clientCommands.missed = NetgameClient::MissedFrame;
    Netgame::CopyInputs(&message.clientCommands.commands, &Netgame::PlayerCommands);

    SEND_MESSAGE_NOACK(message, clientCommands, Netgame::GetServerID());
}

PUBLIC STATIC void Netgame::Update(Uint32 updatesPerFrame) {
    if (!Netgame::Started || Netgame::IsDisconnected())
        return;

    if (Netgame::IsAttemptingConnection())
        Netgame::AttemptConnection();

    if (Netgame::DidConnectionFail())
        return;

    Netgame::Connection->Update();
    if (!Netgame::CheckConnections())
        return;

    if (Netgame::IsConnected())
        Netgame::GetInputs();

    if (Netgame::Server)
        Netgame::SendInputs();

    Netgame::GetMessages();

    if (IS_CLIENT)
        NetgameClient::Update();
    else
        NetgameServer::Update(updatesPerFrame);

    if (Netgame::UseAcks) {
        Packet::ResendMessages();
        Ack::SendAcknowledged();
    }
    else if (Netgame::IsConnected())
        Netgame::Heartbeat();

    if (Server)
        NetgameServer::CheckClients();
}

PUBLIC STATIC bool Netgame::CanRunScene() {
    if (Server && NetgameServer::WaitingClient)
        return false;
    return true;
}
