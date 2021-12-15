#ifndef ENGINE_NETWORK_NETGAME_NETGAMESERVER_H
#define ENGINE_NETWORK_NETGAME_NETGAMESERVER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED



class NetgameServer {
private:
    enum {
    CLIENTSTATE_DISCONNECTED, 
    CLIENTSTATE_WAITING, 
    CLIENTSTATE_READY 
    }; 

    static void SendReady(int client, int playerID);
    static void UpdateJoinTimeout(int client);
    static void UpdateLastTimeCommandReceived(int client);
    static void BuildFrames(Uint32& frameToBuild, Uint32 numFrames);
    static void SendFramesToClient(Uint8 clientNum, Uint32 frameToSend, Uint8 playerCount, Uint8 *playerIDs);
    static void SendFrames(Uint32 frameToSend);

public:
    static ServerClient Clients[MAX_NETWORK_CONNECTIONS];
    static Uint8 MaxPlayerCount;
    static bool JoinsAllowed;
    static bool WaitingClient;
    static Uint32 FrameToBuild;
    static Uint32 FrameToSend;

    static void Cleanup();
    static char* GetClientName(int client);
    static void ReadClientName(Message* message, int client);
    static void ClearClientName(int client);
    static void SendAccept(int client);
    static void SendRefusal(int client, int reason);
    static void SendQuit(int client);
    static void ClientWaiting(Message* message, int client);
    static void ClientReady(int client);
    static void ResendReady(int client);
    static void ClientQuit(int client);
    static bool ClientInGame(int client);
    static bool WaitingReadyFromClient(int client);
    static bool ClientDisconnected(int client);
    static Uint8 GetClientPlayer(int client);
    static void SetClientPlayer(int client, Uint8 player);
    static void UpdateLastTimeContacted(int client);
    static void CheckClients();
    static void AddOwnPlayer();
    static void RemovePlayer(int playerID);
    static void BroadcastPlayerJoin(int playerID);
    static void BroadcastPlayerLeave(int playerID);
    static void BroadcastNameChange(Message* message, int client);
    static void Heartbeat();
    static void Disconnect();
    static void ReadCommands(Message* message, int client);
    static void Update(Uint32 updatesPerFrame);
};

#endif /* ENGINE_NETWORK_NETGAME_NETGAMESERVER_H */
