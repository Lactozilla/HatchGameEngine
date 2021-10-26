#ifndef ENGINE_NETWORK_NETGAME_NETGAME_H
#define ENGINE_NETWORK_NETGAME_NETGAME_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>
#include <Engine/Network/ConnectionManager.h>

class Netgame {
private:
    static bool Restart();
    static bool CreateConnManager();
    static bool InitProtocol(int protocol);
    static bool SendConnectionRequest();
    static void PrepareJoinMessage();
    static bool Receive();
    static bool ProcessMessage(Message* message, int from);
    static bool CheckConnections();
    static bool AttemptConnection();

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

    static bool Start();
    static void Disconnect();
    static void FatalError();
    static bool Stop();
    static void Cleanup();
    static bool StartServer(int protocol, Uint16 port);
    static bool StartServer(Uint16 port);
    static bool StartServer();
    static bool StartUDPServer(Uint16 port);
    static bool StartUDPServer();
    static bool StartTCPServer(Uint16 port);
    static bool StartTCPServer();
    static bool IsDisconnected();
    static bool IsAttemptingConnection();
    static bool IsConnected();
    static bool DidConnectionFail();
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
    static bool Send(int connectionID);
    static void WriteOutgoingData(Uint8* data, size_t length);
    static Uint8* ReadOutgoingData(size_t* length);
    static Uint8* ReadIncomingData(size_t* length);
    static void CloseConnection(int connection, bool sendQuit);
    static void CloseConnection(int connection);
    static NetworkConnection* GetConnection(int connection);
    static NetworkConnection* GetConnection();
    static int GetConnectionID();
    static int GetServerID();
    static void SetServerID(int serverID);
    static bool NewClientConnected();
    static void InsertPlayerIntoSlot(Uint8 playerID, char* name, int client);
    static int AddPlayer(char* name, int client);
    static void RemovePlayer(Uint8 playerID);
    static bool IsPlayerInGame(Uint8 playerID);
    static char* GetPlayerName(Uint8 playerID);
    static bool OnNameChange(Uint8 playerID, char* playerName, bool callback);
    static void ChangePlayerName(Uint8 playerID, char* playerName);
    static void ChangeName(char* playerName);
    static void Heartbeat();
    static void UpdateHeartbeat();
    static void GetMessages();
    static void Callback_SendCommands();
    static void Callback_ReceiveCommands(Uint32 frame);
    static void ClearInputs(NetworkCommands* commands);
    static void CopyInputs(NetworkCommands* dest, NetworkCommands* source);
    static void GetInputs();
    static void SendInputs();
    static void Update(Uint32 updatesPerFrame);
    static bool CanRunScene();
};

#endif /* ENGINE_NETWORK_NETGAME_NETGAME_H */
