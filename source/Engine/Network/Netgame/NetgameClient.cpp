#if INTERFACE
class NetgameClient {
public:
    static bool Accepted;
};
#endif

#include <Engine/Includes/Standard.h>
#include <Engine/Diagnostics/Clock.h>

#include <Engine/Network/Network.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>
#include <Engine/Network/Netgame/Netgame.h>
#include <Engine/Network/Netgame/NetgameClient.h>

bool   NetgameClient::Accepted = false;

PUBLIC STATIC void NetgameClient::Cleanup() {
    NetgameClient::Accepted = false;
}

PUBLIC STATIC void NetgameClient::PrepareJoin() {
    INIT_MESSAGE(message, MESSAGE_JOIN);

    strncpy(message.clientInfo.name, Netgame::ClientName, MAX_CLIENT_NAME);

    WRITE_MESSAGE_NOACK(message, clientInfo, Netgame::GetServerID());
}

PUBLIC STATIC void NetgameClient::SendJoin() {
    NETWORK_DEBUG_IMPORTANT("Sending join to server");

    NetgameClient::PrepareJoin();

    Netgame::Send(Netgame::GetServerID());
}

PUBLIC STATIC void NetgameClient::SendReady() {
    NETWORK_DEBUG_IMPORTANT("Sending ready to server");

    INIT_AND_SEND_MESSAGE_NOACK(MESSAGE_READY, Netgame::GetServerID());
}

PUBLIC STATIC void NetgameClient::ReceiveAccept(Message* message) {
    Netgame::PlayerCount = message->serverInfo.numPlayers;

    NETWORK_DEBUG_IMPORTANT("Received accept from server");
    NetgameClient::Accepted = true;

    NETWORK_DEBUG_IMPORTANT("Player count: %d", Netgame::PlayerCount);

    for (int i = 0; i < MAX_NETGAME_PLAYERS; i++) {
        NetworkPlayer* player = &Netgame::Players[i];

        player->client = CONNECTION_ID_NOBODY;

        if (message->serverInfo.playerInGame[i]) {
            player->ingame = true;

            strncpy(player->name, message->serverInfo.playerNames[i], MAX_CLIENT_NAME);
            player->name[MAX_CLIENT_NAME - 1] = '\0';

            NETWORK_DEBUG_IMPORTANT("Player %d exists (%s)", i, Netgame::GetPlayerName(i));
        }
        else
            player->ingame = false;
    }

    NetgameClient::SendReady();
}

PUBLIC STATIC void NetgameClient::ReceiveRefusal(Message* message) {
    NETWORK_DEBUG_IMPORTANT("Received refusal from server");

    Netgame::Connected = CONNSTATUS_FAILED;
    (void)message; // TODO: Scripting layer callback
}

PUBLIC STATIC bool NetgameClient::WasAccepted() {
    return NetgameClient::Accepted;
}

PUBLIC STATIC void NetgameClient::ReceiveReady(Message* message) {
    NETWORK_DEBUG_IMPORTANT("Received ready from server");

    strncpy(Netgame::ClientName, message->readyInfo.playerName, MAX_CLIENT_NAME);
    Netgame::ClientName[MAX_CLIENT_NAME - 1] = '\0';
    Netgame::PlayerID = message->readyInfo.playerID;

    NETWORK_DEBUG_INFO("Your Player ID: %d", Netgame::PlayerID);
    NETWORK_DEBUG_INFO("Your Player name: %s", Netgame::ClientName);

    if (!NetgameClient::InsertPlayer(Netgame::PlayerID, CONNECTION_ID_SELF, Netgame::ClientName))
        return;

    Netgame::Connected = CONNSTATUS_CONNECTED;
    Netgame::UpdateHeartbeat();
}

#define DISCONNECT_ON_INVALID_PACKET(...) \
    NETWORK_DEBUG_ERROR(__VA_ARGS__); \
    NetgameClient::Disconnect()

#define CHECK_INVALID_PLAYER_ID(playerID) (playerID >= MAX_NETGAME_PLAYERS || playerID == Netgame::PlayerID)

PUBLIC STATIC bool NetgameClient::ReceivePlayerJoin(Message* message) {
    Uint8 playerID = message->playerJoin.id;
    if (CHECK_INVALID_PLAYER_ID(playerID)) {
        DISCONNECT_ON_INVALID_PACKET("Malformed PLAYERJOIN message");
        return false;
    }

    return NetgameClient::InsertPlayer(playerID, CONNECTION_ID_NOBODY, message->playerJoin.name);
}

PUBLIC STATIC bool NetgameClient::ReceivePlayerLeave(Message* message) {
    Uint8 playerID = message->playerLeave.id;
    if (CHECK_INVALID_PLAYER_ID(playerID)) {
        DISCONNECT_ON_INVALID_PACKET("Malformed PLAYERLEAVE message");
        return false;
    }

    Netgame::RemovePlayer(playerID);
    return true;
}

#undef CHECK_INVALID_PLAYER_ID

PUBLIC STATIC void NetgameClient::ReceiveNameChange(Message* message) {
    NETWORK_DEBUG_IMPORTANT("Received name change from server");

    Uint8 playerID = message->nameChange.playerID;
    if (playerID >= Netgame::PlayerCount) {
        DISCONNECT_ON_INVALID_PACKET("Invalid player ID %d", playerID);
        return;
    }

    Netgame::ChangePlayerName(playerID, message->nameChange.name);
}

PRIVATE STATIC bool NetgameClient::InsertPlayer(Uint8 playerID, int connectionID, char* name) {
    if (playerID >= MAX_NETGAME_PLAYERS) {
        DISCONNECT_ON_INVALID_PACKET("Invalid player ID %d", playerID);
        return false;
    }

    Netgame::PlayerCount++;
    Netgame::InsertPlayerIntoSlot(playerID, name, connectionID);

    // TODO: Scripting layer callback

    return true;
}

#undef DISCONNECT_ON_INVALID_PACKET

PUBLIC STATIC void NetgameClient::Heartbeat() {
    INIT_AND_SEND_MESSAGE_NOACK(MESSAGE_HEARTBEAT, Netgame::GetServerID());
}

PUBLIC STATIC void NetgameClient::Disconnect() {
    Netgame::CloseConnection(Netgame::GetServerID());
    Netgame::Cleanup();

    if (Netgame::IsConnected())
        Netgame::Connected = CONNSTATUS_DISCONNECTED;
    else
        Netgame::Connected = CONNSTATUS_FAILED;
}
