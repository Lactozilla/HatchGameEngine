#ifndef ENGINE_NETWORK_NETGAME_NETGAMECLIENT_H
#define ENGINE_NETWORK_NETGAME_NETGAMECLIENT_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED



class NetgameClient {
private:
    static bool InsertPlayer(Uint8 playerID, int connectionID, char* name);

public:
    static bool Accepted;

    static void Cleanup();
    static void PrepareJoin();
    static void SendJoin();
    static void SendReady();
    static void ReceiveAccept(Message* message);
    static void ReceiveRefusal(Message* message);
    static bool WasAccepted();
    static void ReceiveReady(Message* message);
    static bool ReceivePlayerJoin(Message* message);
    static bool ReceivePlayerLeave(Message* message);
    static void ReceiveNameChange(Message* message);
    static void Heartbeat();
    static void Disconnect();
};

#endif /* ENGINE_NETWORK_NETGAME_NETGAMECLIENT_H */
