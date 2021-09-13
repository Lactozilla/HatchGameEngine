#if INTERFACE
class NetgameServer {
    enum {
        CLIENTSTATE_DISCONNECTED,
        CLIENTSTATE_WAITING,
        CLIENTSTATE_READY
    };
public:
    static ServerClient Clients[MAX_NETWORK_CONNECTIONS];
    static Uint8 MaxPlayerCount;
    static bool JoinsAllowed;
};
#endif

#include <Engine/Includes/Standard.h>
#include <Engine/Diagnostics/Clock.h>

#include <Engine/Network/Network.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>
#include <Engine/Network/Netgame/Netgame.h>
#include <Engine/Network/Netgame/NetgameServer.h>

ServerClient NetgameServer::Clients[MAX_NETWORK_CONNECTIONS];
Uint8        NetgameServer::MaxPlayerCount = MAX_NETGAME_PLAYERS;
bool         NetgameServer::JoinsAllowed = true;

#define INDEX_CLIENT(i) NetgameServer::Clients[i]
#define CLIENT_STATE(clientNum) INDEX_CLIENT(clientNum).State
#define CLIENT_NAME(clientNum) INDEX_CLIENT(clientNum).Name
#define CLIENT_TIMEOUT(clientNum) INDEX_CLIENT(clientNum).Timeout
#define CLIENT_JOIN_TIMEOUT(clientNum) INDEX_CLIENT(clientNum).JoinTimeout
#define CLIENT_READY_RESENT(clientNum) INDEX_CLIENT(clientNum).ReadyResent
#define CLIENT_PLAYER(clientNum) INDEX_CLIENT(clientNum).PlayerID

PUBLIC STATIC void NetgameServer::Cleanup() {
    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        CLIENT_STATE(i) = CLIENTSTATE_DISCONNECTED;
        CLIENT_TIMEOUT(i) = CLIENT_JOIN_TIMEOUT(i) = 0.0;
        CLIENT_READY_RESENT(i) = 0;
        CLIENT_PLAYER(i) = CONNECTION_ID_NOBODY;
        memset(CLIENT_NAME(i), 0x00, MAX_CLIENT_NAME);
    }
}

PUBLIC STATIC char* NetgameServer::GetClientName(int client) {
    return CLIENT_NAME(client);
}

PUBLIC STATIC void NetgameServer::ReadClientName(Message* message, int client) {
    strncpy(CLIENT_NAME(client), message->clientInfo.name, MAX_CLIENT_NAME);
    CLIENT_NAME(client)[MAX_CLIENT_NAME - 1] = '\0';
}

PUBLIC STATIC void NetgameServer::ClearClientName(int client) {
    memset(CLIENT_NAME(client), 0x00, MAX_CLIENT_NAME);
}

PUBLIC STATIC void NetgameServer::SendAccept(int client) {
    INIT_MESSAGE(message, MESSAGE_ACCEPT);

    message.serverInfo.numPlayers = Netgame::PlayerCount;

    for (int i = 0; i < MAX_NETGAME_PLAYERS; i++) {
        if (Netgame::Players[i].ingame) {
            message.serverInfo.playerInGame[i] = 1;
            strncpy(message.serverInfo.playerNames[i], Netgame::Players[i].name, MAX_CLIENT_NAME);
            message.serverInfo.playerNames[i][MAX_CLIENT_NAME - 1] = '\0';
        }
        else {
            message.serverInfo.playerInGame[i] = 0;
        }
    }

    NETWORK_DEBUG_IMPORTANT("Sending accept message to client %d (%s)", client, CLIENT_NAME(client));

    NetgameServer::UpdateJoinTimeout(client);

    SEND_MESSAGE_NOACK(message, serverInfo, client);
}

PUBLIC STATIC void NetgameServer::SendRefusal(int client, int reason) {
    INIT_MESSAGE(message, MESSAGE_REFUSE);

    NETWORK_DEBUG_IMPORTANT("Sending refusal message to client %d (%s)", client, CLIENT_NAME(client));

    message.refuseInfo.reason = reason;
    SEND_MESSAGE(message, refuseInfo, client);

    NETWORK_DEBUG_IMPORTANT("Closing connection to refused client");
    Netgame::CloseConnection(client);
}

PRIVATE STATIC void NetgameServer::SendReady(int client, int playerID) {
    INIT_MESSAGE(message, MESSAGE_READY);

    NETWORK_DEBUG_IMPORTANT("Sending ready message to client %d (%s), player ID %d", client, CLIENT_NAME(client), playerID);

    strncpy(message.readyInfo.playerName, Netgame::GetPlayerName(playerID), MAX_CLIENT_NAME);
    message.readyInfo.playerName[MAX_CLIENT_NAME - 1] = '\0';
    message.readyInfo.playerID = playerID;

    SEND_MESSAGE(message, readyInfo, client);
}

PUBLIC STATIC void NetgameServer::SendQuit(int client) {
    NETWORK_DEBUG_IMPORTANT("Sending quit message to client %d (%s)", client, CLIENT_NAME(client));

    INIT_AND_SEND_MESSAGE_NOACK(MESSAGE_QUIT, client); // I hope this message finds you well
}

PUBLIC STATIC void NetgameServer::ClientWaiting(Message* message, int client) {
    NetgameServer::ReadClientName(message, client);

    CLIENT_STATE(client) = CLIENTSTATE_WAITING;

    NETWORK_DEBUG_IMPORTANT("Client %d (%s) is now waiting", client, CLIENT_NAME(client));
}

PUBLIC STATIC void NetgameServer::ClientReady(int client) {
    NETWORK_DEBUG_IMPORTANT("Client %d (%s) is ready", client, CLIENT_NAME(client));

    int playerID = Netgame::AddPlayer(CLIENT_NAME(client), client);
    if (playerID == -1) {
        NETWORK_DEBUG_IMPORTANT("No free player slot found for client %d (%s)", client, CLIENT_NAME(client));
        Netgame::CloseConnection(client);
        return;
    }

    CLIENT_STATE(client) = CLIENTSTATE_READY;
    NetgameServer::BroadcastPlayerJoin(playerID);

    NetgameServer::SendReady(client, playerID);
    NetgameServer::ClearClientName(client);
}

PUBLIC STATIC void NetgameServer::ResendReady(int client) {
    if (CLIENT_READY_RESENT(client) >= 10) {
        NETWORK_DEBUG_IMPORTANT("Resent ready to client %d (%s) too many times, disconnecting", client, CLIENT_NAME(client));
        Netgame::CloseConnection(client);
        return;
    }

    // NETWORK_DEBUG_IMPORTANT("Resending ready to client %d (%s)", client, CLIENT_NAME(client));

    NetgameServer::SendReady(client, CLIENT_PLAYER(client));

    CLIENT_READY_RESENT(client)++;
}

#define CHECK_SELF_CLIENT(ret) \
    if (client == CONNECTION_ID_SELF) \
        return ret;

PUBLIC STATIC void NetgameServer::ClientQuit(int client) {
    CHECK_SELF_CLIENT();
    CLIENT_STATE(client) = CLIENTSTATE_DISCONNECTED;
}

PUBLIC STATIC bool NetgameServer::ClientInGame(int client) {
    CHECK_SELF_CLIENT(true);
    return (CLIENT_STATE(client) == CLIENTSTATE_READY);
}

PUBLIC STATIC bool NetgameServer::WaitingReadyFromClient(int client) {
    CHECK_SELF_CLIENT(false);
    return (CLIENT_STATE(client) == CLIENTSTATE_WAITING);
}

PUBLIC STATIC bool NetgameServer::ClientDisconnected(int client) {
    CHECK_SELF_CLIENT(false);
    return (CLIENT_STATE(client) == CLIENTSTATE_DISCONNECTED);
}

PUBLIC STATIC Uint8 NetgameServer::GetClientPlayer(int client) {
    CHECK_SELF_CLIENT(Netgame::PlayerID);
    return CLIENT_PLAYER(client);
}

PUBLIC STATIC void NetgameServer::SetClientPlayer(int client, Uint8 player) {
    CHECK_SELF_CLIENT();
    CLIENT_PLAYER(client) = player;
}

PUBLIC STATIC void NetgameServer::UpdateLastTimeContacted(int client) {
    CHECK_SELF_CLIENT();
    CLIENT_TIMEOUT(client) = Clock::GetTicks();
}

PRIVATE STATIC void NetgameServer::UpdateJoinTimeout(int client) {
    CHECK_SELF_CLIENT();
    CLIENT_JOIN_TIMEOUT(client) = Clock::GetTicks();
}

#undef CHECK_SELF_CLIENT

PUBLIC STATIC void NetgameServer::CheckClients() {
    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (NetgameServer::ClientDisconnected(i))
            continue;

        double elapsed;

        if (NetgameServer::WaitingReadyFromClient(i)) {
            elapsed = Clock::GetTicks() - CLIENT_JOIN_TIMEOUT(i);

            if (elapsed >= CLIENT_WAITING_INTERVAL)
                NetgameServer::SendAccept(i);
        }

        elapsed = Clock::GetTicks() - CLIENT_TIMEOUT(i);

        if (elapsed >= CLIENT_TIMEOUT_SECS * 1000.0) {
            NETWORK_DEBUG_IMPORTANT("Client %d (%s) took too long to respond", i, CLIENT_NAME(i));
            Netgame::CloseConnection(i, false);
        }
    }
}

PUBLIC STATIC void NetgameServer::AddOwnPlayer() {
    Netgame::PlayerID = Netgame::AddPlayer(Netgame::ClientName, CONNECTION_ID_SELF);
    // TODO: Scripting layer callback
}

PUBLIC STATIC void NetgameServer::RemovePlayer(int playerID) {
    NetgameServer::BroadcastPlayerLeave(playerID);
    Netgame::RemovePlayer(playerID);
}

PUBLIC STATIC void NetgameServer::BroadcastPlayerJoin(int playerID) {
    INIT_MESSAGE(broadcast, MESSAGE_PLAYERJOIN);

    broadcast.playerJoin.id = playerID;
    strncpy(broadcast.playerJoin.name, Netgame::Players[playerID].name, MAX_CLIENT_NAME);

    NETWORK_DEBUG_IMPORTANT("Broadcasting new player to all clients");

    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (NetgameServer::ClientDisconnected(i))
            continue;

        if (CLIENT_PLAYER(i) == playerID)
            continue;

        SEND_MESSAGE(broadcast, playerJoin, i);
    }
}

PUBLIC STATIC void NetgameServer::BroadcastPlayerLeave(int playerID) {
    INIT_MESSAGE(broadcast, MESSAGE_PLAYERLEAVE);

    broadcast.playerLeave.id = playerID;

    NETWORK_DEBUG_IMPORTANT("Broadcasting player leave to all clients");

    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (NetgameServer::ClientDisconnected(i))
            continue;

        if (CLIENT_PLAYER(i) == playerID)
            continue;

        SEND_MESSAGE(broadcast, playerJoin, i);
    }
}

PUBLIC STATIC void NetgameServer::BroadcastNameChange(Message* message, int client) {
    INIT_MESSAGE(broadcast, MESSAGE_NAMECHANGE);

    int playerID = NetgameServer::GetClientPlayer(client);
    if (!Netgame::OnNameChange(playerID, message->nameChange.name, true))
        return;

    NetworkPlayer* player = &Netgame::Players[playerID];
    Netgame::ChangePlayerName(playerID, player->pendingName);

    broadcast.nameChange.playerID = playerID;
    strncpy(broadcast.nameChange.name, player->pendingName, MAX_CLIENT_NAME);

    NETWORK_DEBUG_IMPORTANT("Broadcasting name change to all clients");

    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (!NetgameServer::ClientDisconnected(i)) {
            SEND_MESSAGE(broadcast, nameChange, i);
        }
    }
}

PUBLIC STATIC void NetgameServer::Heartbeat() {
    INIT_MESSAGE(heartbeat, MESSAGE_HEARTBEAT);

    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (NetgameServer::ClientInGame(i))
            SEND_MESSAGE_ZEROLENGTH(heartbeat, i);
    }
}

PUBLIC STATIC void NetgameServer::Disconnect() {
    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (!NetgameServer::ClientDisconnected(i)) {
            Netgame::CloseConnection(i);
        }
    }
}

#undef INDEX_CLIENT
#undef CLIENT_STATE
#undef CLIENT_NAME
#undef CLIENT_TIMEOUT
#undef CLIENT_JOIN_TIMEOUT
#undef CLIENT_PLAYER
