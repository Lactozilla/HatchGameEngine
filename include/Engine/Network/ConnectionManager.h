#ifndef ENGINE_NETWORK_CONNECTIONMANAGER_H
#define ENGINE_NETWORK_CONNECTIONMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class SocketManager;

#include <Engine/Includes/Standard.h>
#include <Engine/Network/Connection.h>
#include <Engine/Network/Network.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/SocketManager.h>
#include <Engine/Network/Socket/Includes.h>

class ConnectionManager {
private:
    void InitLoopback();
    void InitConnection(NetworkConnection* conn);
    int CreateConnection();
    void RemoveConnection(int connectionNum);
    bool CompareLoopback(SocketAddress* sockAddress);
    bool DropMessage();
    bool ReceiveOwnMessages();
    int ReceiveUDP();
    bool Accept();
    int ReceiveTCP();
    bool ReceiveFromTCPConnection();
    void SendToSelf();

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

    static ConnectionManager* New();
    bool SetUDP();
    bool SetTCP();
    bool Init(Uint16 port);
    bool StartUDP(Uint16 port);
    bool StartTCP(Uint16 port);
    bool Start(Uint16 port);
    bool StartUDPServer(Uint16 port);
    bool StartTCPServer(Uint16 port);
    bool StartServer(Uint16 port);
    int GetSocketStatus(int* activeSockets, bool reconnect);
    void RemoveSocketsExcept(int connSocket);
    bool Connect(const char* address, Uint16 port);
    bool Reconnect();
    bool SetConnectionMessage(Uint8* message, size_t messageLength);
    void CloseConnection(int connectionNum);
    void SetPacketDropPercentage(float percentage);
    bool Send(int connectionNum);
    bool Send(int connectionNum, size_t length);
    bool Send();
    void WriteOutgoingData(Uint8* data, size_t length);
    Uint8* ReadOutgoingData(size_t* length);
    Uint8* ReadIncomingData(size_t* length);
    int Receive();
    char* GetAddress(int connectionNum);
    char* GetAddress();
    void Update();
    void Dispose();
};

#endif /* ENGINE_NETWORK_CONNECTIONMANAGER_H */
