#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Connection.h>

class Ack {
public:

};
#endif

#include <Engine/Includes/Standard.h>
#include <Engine/Diagnostics/Clock.h>

#include <Engine/Network/Ack.h>
#include <Engine/Network/Message.h>
#include <Engine/Network/Connection.h>
#include <Engine/Network/Netgame/Netgame.h>

#define ACK_RESEND_INTERVAL NETWORK_HEARTBEAT_INTERVAL

#define ACK_QUEUE_NEXT(q) ( (q + 1) % ACK_SEQUENCE_BUFFER_SIZE )
#define ACK_QUEUE_PREV(q) ( (q + (ACK_SEQUENCE_BUFFER_SIZE - 1) ) % ACK_SEQUENCE_BUFFER_SIZE )

PUBLIC STATIC void Ack::GetNext(AckNum* ack) {
    (*ack)++;
    if (!(*ack))
        (*ack) = 1;
}

#define ackQueue connection->ack.queue
#define ackRing(ringPos) ackQueue.ring[ringPos]

PRIVATE STATIC bool Ack::QueueInsert(AckNum ack, NetworkConnection* connection) {
    if (Ack::Compare(ack, connection->ack.lastReceived) != ACKCMP_HIGHER) {
        return false;
    }

    AckNum i = ackQueue.tail;
    for (; i != ackQueue.head; i = ACK_QUEUE_NEXT(i)) {
        if (ack == ackRing(i))
            return false;
    }

    AckNum nextAck = connection->ack.lastReceived;
    Ack::GetNext(&nextAck);

    if (ack != nextAck) {
        return Ack::QueueInsertOutOfOrder(ack, connection);
    }
    else {
        Ack::GetNext(&nextAck);
        connection->ack.lastReceived = ack;
    }

    AckNum head = ACK_QUEUE_PREV(ackQueue.head);

    for (;;) {
        bool done = true;

        for (i = ackQueue.tail; i != ackQueue.head; i = ACK_QUEUE_NEXT(i)) {
            bool onHead = (head == i);

            if (Ack::Compare(ackRing(i), nextAck) == ACKCMP_HIGHER)
                continue;

            if (ackQueue.tail == i) {
                ackRing(ackQueue.tail) = 0;
                ackQueue.tail = ACK_QUEUE_NEXT(i);
            }
            else if (onHead) {
                ackRing(ackQueue.head) = 0;
                ackQueue.head = head;
                head = ACK_QUEUE_PREV(head);
            }

            if (ackRing(i) == nextAck) {
                connection->ack.lastReceived = nextAck;
                Ack::GetNext(&nextAck);
                done = false;
            }

            if (onHead)
                break;
        }

        if (done)
            break;
    }

    return true;
}

PRIVATE STATIC bool Ack::QueueInsertOutOfOrder(AckNum ack, NetworkConnection* connection) {
    AckNum head = ACK_QUEUE_NEXT(ackQueue.head);
    if (head == ackQueue.tail)
        return false;

    ackRing(ackQueue.head) = ack;
    ackQueue.head = head;

    return true;
}

#undef ackQueue
#undef ackRing

PUBLIC STATIC AckNum Ack::GetLastReceived(int connectionID) {
    NetworkConnection* connection = Netgame::GetConnection(connectionID);
    if (!connection)
        return 0;

    AckNum ack = connection->ack.lastReceived;
    connection->ack.lastTimeSent = Clock::GetTicks();

    return ack;
}

PUBLIC STATIC bool Ack::Process(Message* message) {
    int connectionID = Netgame::GetConnectionID();
    NetworkConnection* connection = Netgame::GetConnection(connectionID);

    AckNum lastReceived = message->ack.lastReceived;
    if (lastReceived && Ack::Compare(connection->ack.lastSeqAcknowledged, lastReceived) == ACKCMP_LESS) {
        Ack::ProcessLastReceived(connectionID, lastReceived);
    }

    int ack = message->ack.sequence;
    if (ack) {
        return Ack::QueueInsert(ack, connection);
    }

    return true;
}

PRIVATE STATIC void Ack::ProcessLastReceived(int connectionID, AckNum lastReceived) {
    NetworkConnection* connection = Netgame::GetConnection(connectionID);
    connection->ack.lastSeqAcknowledged = lastReceived;
    Packet::ResendListIterate(connectionID, lastReceived, Ack::AcknowledgeFromMessage);
}

PRIVATE STATIC void Ack::AcknowledgeFromMessage(int curAck, int ackNum) {
    if (Ack::Compare(curAck, ackNum) == ACKCMP_HIGHER)
        return;

    Packet::ResendListRemove(curAck);
}

PRIVATE STATIC AckCompare Ack::Compare(AckNum a, AckNum b) {
    Sint32 diff = a - b;

    if (diff >= ACKCMP_DIFF_MAX || diff < ACKCMP_DIFF_MIN)
        diff = -diff;

    if (diff < 0)
        return ACKCMP_LESS;
    else if (diff > 0)
        return ACKCMP_HIGHER;
    else
        return ACKCMP_EQUAL;
}

PUBLIC STATIC void Ack::SendAcknowledged() {
    for (int i = 0; i < MAX_NETWORK_CONNECTIONS; i++) {
        NetworkConnection* connection = Netgame::GetConnection(i);
        if (!connection)
            continue;

        AckNum ack = connection->ack.lastReceived;
        if (!ack) {
            INIT_AND_SEND_MESSAGE_NOACK(MESSAGE_HEARTBEAT, i);
            continue;
        }

        double time = Clock::GetTicks() - connection->ack.lastTimeSent;
        if (time >= ACK_RESEND_INTERVAL) {
            INIT_MESSAGE(message, MESSAGE_ACKNOWLEDGEMENT);

            for (int a = 0; a < ACK_SEQUENCE_BUFFER_SIZE; a++) {
                int ackNum = connection->ack.queue.ring[a];
                if (ackNum) {
                    message.ackList.list[message.ackList.numAcks] = ackNum;
                    message.ackList.numAcks++;
                }
            }

            SEND_MESSAGE_NOACK(message, ackList, i);
        }
    }
}

PUBLIC STATIC void Ack::Acknowledge(Message* message, int connectionID) {
    for (int i = 0; i < message->ackList.numAcks; i++) {
        Packet::ResendListIterate(connectionID, message->ackList.list[i], Ack::RemoveAcknowledged);
    }
}

PRIVATE STATIC void Ack::RemoveAcknowledged(int curAck, int ackNum) {
    if (curAck == ackNum)
        Packet::ResendListRemove(curAck);
}
