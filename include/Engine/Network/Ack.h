#ifndef ENGINE_NETWORK_ACK_H
#define ENGINE_NETWORK_ACK_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Connection.h>

class Ack {
private:
    static bool QueueInsert(AckNum ack, NetworkConnection* connection);
    static bool QueueInsertOutOfOrder(AckNum ack, NetworkConnection* connection);
    static void ProcessLastReceived(int connectionID, AckNum lastReceived);
    static void AcknowledgeFromMessage(int curAck, int ackNum);
    static AckCompare Compare(AckNum a, AckNum b);
    static void RemoveAcknowledged(int curAck, int ackNum);

public:
    static void GetNext(AckNum* ack);
    static AckNum GetLastReceived(int connectionID);
    static bool Process(Message* message);
    static void SendAcknowledged();
    static void Acknowledge(Message* message, int connectionID);
};

#endif /* ENGINE_NETWORK_ACK_H */
