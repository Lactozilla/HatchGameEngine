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
    static bool WaitingClient;

    static Uint32 FrameToBuild;
    static Uint32 FrameToSend;
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
bool         NetgameServer::WaitingClient = false;

Uint32       NetgameServer::FrameToBuild = 0;
Uint32       NetgameServer::FrameToSend = 0;

#define INDEX_CLIENT(i) NetgameServer::Clients[i]
#define CLIENT_STATE(clientNum) INDEX_CLIENT(clientNum).State
#define CLIENT_NAME(clientNum) INDEX_CLIENT(clientNum).Name
#define CLIENT_TIMEOUT(clientNum) INDEX_CLIENT(clientNum).Timeout
#define CLIENT_COMMAND_TIMEOUT(clientNum) INDEX_CLIENT(clientNum).CommandTimeout
#define CLIENT_JOIN_TIMEOUT(clientNum) INDEX_CLIENT(clientNum).JoinTimeout
#define CLIENT_READY_RESENT(clientNum) INDEX_CLIENT(clientNum).ReadyResent
#define CLIENT_PLAYER(clientNum) INDEX_CLIENT(clientNum).PlayerID

PUBLIC STATIC void NetgameServer::Cleanup() {
    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        ServerClient* cl = &NetgameServer::Clients[i];
        cl->State = CLIENTSTATE_DISCONNECTED;
        cl->Timeout = cl->JoinTimeout = cl->CommandTimeout = 0.0;
        cl->ReadyResent = 0;
        cl->Frame = cl->NextFrame = 0;
        cl->PlayerID = CONNECTION_ID_NOBODY;
        memset(cl->Name, 0x00, MAX_CLIENT_NAME);
    }

    NetgameServer::WaitingClient = false;
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
    message.readyInfo.serverFrame = Packet::SwapLong(Netgame::CurrentFrame);

    NETWORK_DEBUG_IMPORTANT("Frame: %d", Netgame::CurrentFrame);

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

    ServerClient* cl = &NetgameServer::Clients[client];
    cl->State = CLIENTSTATE_READY;

    NetgameServer::BroadcastPlayerJoin(playerID);

    NetgameServer::SendReady(client, playerID);
    NetgameServer::ClearClientName(client);

    cl->Frame = cl->NextFrame = Netgame::CurrentFrame;

    NetgameServer::UpdateLastTimeCommandReceived(client);
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

PRIVATE STATIC void NetgameServer::UpdateLastTimeCommandReceived(int client) {
    CHECK_SELF_CLIENT();
    CLIENT_COMMAND_TIMEOUT(client) = Clock::GetTicks();
}

#undef CHECK_SELF_CLIENT

PUBLIC STATIC void NetgameServer::CheckClients() {
    NetgameServer::WaitingClient = false;

    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (NetgameServer::ClientDisconnected(i))
            continue;

        double elapsed;

        if (NetgameServer::WaitingReadyFromClient(i)) {
            elapsed = Clock::GetTicks() - CLIENT_JOIN_TIMEOUT(i);

            if (elapsed >= CLIENT_WAITING_INTERVAL)
                NetgameServer::SendAccept(i);

            NetgameServer::WaitingClient = true;
        }

        elapsed = Clock::GetTicks() - CLIENT_TIMEOUT(i);

        if (elapsed >= CLIENT_TIMEOUT_SECS * 1000.0) {
            NETWORK_DEBUG_IMPORTANT("Client %d (%s) took too long to respond", i, CLIENT_NAME(i));
            Netgame::CloseConnection(i, false);
            continue;
        }

        if (NetgameServer::ClientInGame(i)) {
            elapsed = Clock::GetTicks() - CLIENT_COMMAND_TIMEOUT(i);

            if (elapsed >= CLIENT_TIMEOUT_SECS * 1000.0) {
                NETWORK_DEBUG_IMPORTANT("Client %d (%s) took too long to send a command", i, CLIENT_NAME(i));
                Netgame::CloseConnection(i);
            }
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

PUBLIC STATIC void NetgameServer::ReadCommands(Message* message, int client) {
    NetworkPlayer* player;
    Uint8 playerID;
    ServerClient* cl;

    Uint32 serverFrame = (NetgameServer::FrameToBuild % INPUT_BUFFER_FRAMES);
    Uint32 frame = serverFrame;

    if (client == CONNECTION_ID_SELF) {
        player = &Netgame::Players[(playerID = Netgame::PlayerID)];
        goto copyCommands;
    }

    cl = &NetgameServer::Clients[client];
    player = &Netgame::Players[(playerID = cl->PlayerID)];
    if (!Netgame::IsPlayerInGame(playerID))
        return;

    frame = message->clientCommands.frame;

    if (message->clientCommands.missed || cl->NextFrame < frame)
        cl->NextFrame = frame;

    if (cl->Frame > frame) {
        return;
    }

    cl->Frame = frame;

    NetgameServer::UpdateLastTimeCommandReceived(client);

copyCommands:
    Netgame::CopyInputs(&player->commands[serverFrame], &message->clientCommands.commands);
    player->receivedCommands[serverFrame] = true;

    NETWORK_DEBUG_FRAME("Got frame %d from %d (length: %d, server frame: %d)",
         frame, playerID, player->commands[serverFrame].numCommands, serverFrame);
}

PRIVATE STATIC void NetgameServer::BuildFrames(Uint32& frameToBuild, Uint32 numFrames) {
    NETWORK_DEBUG_FRAME("Building from frame %d to %d", frameToBuild, frameToBuild + numFrames - 1);

    for (int i = 0; i < numFrames; i++) {
        Uint32 frame = frameToBuild % INPUT_BUFFER_FRAMES;
        Uint32 lastFrame = frameToBuild ? (frameToBuild-1) % INPUT_BUFFER_FRAMES : 0;

        for (Uint8 j = 0; j < Netgame::PlayerCount; j++) {
            if (!Netgame::IsPlayerInGame(j))
                continue;

            NetworkPlayer* player = &Netgame::Players[j];

            NETWORK_DEBUG_FRAME("Building frame %d for player %d (received: %d)",
                 frame, j, player->receivedCommands[frame]);

            if (!player->receivedCommands[frame])
                Netgame::CopyInputs(&player->commands[frame], &player->commands[lastFrame]);
        }

        frameToBuild++;
    }
}

PRIVATE STATIC void NetgameServer::SendFrames(Uint32 frameToSend) {
    Uint8 playerCount = 0;
    Uint8 playerIDs[MAX_NETGAME_PLAYERS];

    for (Uint8 j = 0; j < Netgame::PlayerCount; j++) {
        if (!Netgame::IsPlayerInGame(j))
            continue;

        playerIDs[playerCount] = j;
        playerCount++;
    }

    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (!NetgameServer::ClientInGame(i))
            continue;

        ServerClient* cl = &NetgameServer::Clients[i];

        Uint32 clFrame = cl->Frame;
        Uint32 maxFrames = clFrame + MESSAGE_INPUT_BUFFER_FRAMES;
        Uint32 end = NetgameServer::FrameToBuild;

        if (end >= maxFrames)
            end = maxFrames;

        // Send from the client's NEXT frame, until the frame we need to build
        Uint32 start = cl->NextFrame;

        // Nothing to send?
        if (start >= end) {
            start = clFrame; // Send from the client's CURRENT frame, until the frame we need to build
            if (start >= end) // No need to send anything then
                continue;
        }

        cl->NextFrame = end;

        if (end-1 > start)
            cl->NextFrame = end-1;

        // Somehow less than cl->Frame?
        if (cl->NextFrame < clFrame)
            cl->NextFrame = clFrame;

        // Don't send old frames
        if (start < frameToSend)
            start = frameToSend;

        INIT_MESSAGE(message, MESSAGE_SERVERCOMMANDS);

        // Check the message size.
        Uint8* cmdBuffer = (Uint8*)(&(message.serverCommands.commands[0][0]));
        Uint32 msgSize = (cmdBuffer - (Uint8*)(&message));
        Uint32 bufSize = SOCKET_BUFFER_LENGTH - MESSAGE_HEADER_LENGTH;
        Uint32 cmdSize = sizeof(NetworkCommands) * playerCount;

        for (Uint32 frame = start; frame < end; frame++) {
            msgSize += cmdSize;

            // Send up until the last frame, since there's no more space
            if (msgSize >= bufSize) {
                msgSize = bufSize - 1;
                end = frame ? (frame - 1) : 0;
                if (end == start)
                    end++;
                break;
            }
        }

        // Copy the commands
        for (Uint8 j = 0; j < playerCount; j++) {
            Uint8 playerID = playerIDs[j];

            for (Uint32 frame = start; frame < end; frame++) {
                Netgame::CopyInputs((NetworkCommands*)cmdBuffer, &Netgame::Players[playerID].commands[frame % INPUT_BUFFER_FRAMES]);
                cmdBuffer += sizeof(NetworkCommands);
            }

            message.serverCommands.playerIDs[j] = playerID;
        }

        message.serverCommands.numPlayers = playerCount;
        message.serverCommands.startFrame = Packet::SwapLong(start);
        message.serverCommands.endFrame = Packet::SwapLong(end);

        NETWORK_DEBUG_FRAME("Sending frames %d through %d to client %d (size: %d)", start, end-1, i, msgSize);

        STORE_MESSAGE_IN_PACKET(message);
        packetToSend.Send(i, msgSize, false);
    }

    NetgameServer::FrameToSend = frameToSend;
}

PUBLIC STATIC void NetgameServer::Update(Uint32 updatesPerFrame) {
    Uint32 frameToBuild = NetgameServer::FrameToBuild;
    Uint32 frameToSend = Netgame::CurrentFrame;

    // Find the oldest frame to send
    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        if (!NetgameServer::ClientInGame(i))
            continue;

        Uint32 clientFrame = NetgameServer::Clients[i].Frame;
        if (clientFrame < frameToSend)
            frameToSend = clientFrame;
    }

    // This stops the server from running if one of the clients is behind.
    // Let's say client A is at frame 100, and the server is at frame 170.
    // Assuming that the input buffer is only 60 frames large,
    // let inputBufferSize = 60; maxFramesToSend = 100 + inputBufferSize;
    // and (serverFrame = 170 + updatesPerFrame) >= maxFramesToSend.
    // The result will be 170 >= 160. numFrames will then be 160 - 170 - 1.
    // numFrames will be less than one, which means no frames will be run
    // until the client catches up with the server, as the server sends
    // frames that the client hasn't yet received.
    Uint32 maxFramesToSend = frameToSend + INPUT_BUFFER_FRAMES;
    Uint32 serverFrame = frameToBuild + updatesPerFrame;
    Sint32 numFrames;

    if (serverFrame >= maxFramesToSend)
        numFrames = maxFramesToSend - frameToBuild - 1;
    else
        numFrames = updatesPerFrame;

    if (numFrames > 0) {
        NetgameServer::BuildFrames(frameToBuild, numFrames);
        NetgameServer::FrameToBuild = frameToBuild;
    }

    NetgameServer::SendFrames(frameToSend);
    Netgame::NextFrame = frameToBuild;
}

#undef INDEX_CLIENT
#undef CLIENT_STATE
#undef CLIENT_NAME
#undef CLIENT_TIMEOUT
#undef CLIENT_JOIN_TIMEOUT
#undef CLIENT_PLAYER
