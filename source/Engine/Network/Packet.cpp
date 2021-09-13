#if INTERFACE
class Packet {
public:
    Message message;
    int messageLength;
    PacketRetransmission resend;
};
#endif

#include <Engine/Includes/Standard.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Hashing/CRC32.h>

#include <Engine/Network/Message.h>
#include <Engine/Network/Packet.h>
#include <Engine/Network/Ack.h>

#include <Engine/Network/Network.h>
#include <Engine/Network/Netgame/Netgame.h>
#include <Engine/Network/Netgame/NetgameClient.h>

// Initializes a packet
PUBLIC void Packet::Init() {
    resend.ackNum = resend.ackSequence = 0;
    resend.timesResent = 0;
    resend.sendTo = 0;
    resend.sentAt = 0.0;
}

// Prepares an outgoing message, but doesn't send it.
PUBLIC bool Packet::PrepareMessage(int connection, size_t length, bool useAck) {
    messageLength = (int)length + MESSAGE_HEADER_LENGTH;
    if (messageLength >= SOCKET_BUFFER_LENGTH)
        return false;

    Checksum();

    if (Netgame::UseAcks && connection != CONNECTION_ID_SELF) {
        if (useAck && !GetAck(connection))
            return false;

        message.ack.lastReceived = Ack::GetLastReceived(connection);
    }

    Netgame::WriteOutgoingData((Uint8*)(&message), messageLength);

    return true;
}

// Sends a message
PUBLIC bool Packet::Send(int connection, size_t length, bool useAck) {
    if (!Netgame::GetConnection(connection))
        return false;

    if (!PrepareMessage(connection, length, useAck))
        return false;

    Netgame::Send(connection);

    return true;
}

// Receives a packet and processes its message
PUBLIC STATIC bool Packet::Process(Message* outMessage) {
    Uint8* data;
    size_t length;

    data = Netgame::ReadIncomingData(&length);
    if (length < MESSAGE_HEADER_LENGTH)
        return false;

    memcpy(outMessage, data, length);

    if (length > MESSAGE_HEADER_LENGTH) {
        length -= MESSAGE_HEADER_LENGTH;
        if (outMessage->checksum != Packet::ChecksumPayload((Uint8*)outMessage, length))
            return false;
    }

    if (Netgame::UseAcks) {
        if (!Ack::Process(outMessage))
            return false;
    }

    return true;
}

// Generates the checksum for a payload
PUBLIC void Packet::Checksum() {
    int length = messageLength - MESSAGE_HEADER_LENGTH;
    if (length <= 0) {
        message.checksum = 0;
        return;
    }

    message.checksum = Packet::ChecksumPayload((Uint8*)(&message), length);
}

PRIVATE STATIC Uint32 Packet::ChecksumPayload(Uint8* message, size_t length) {
    return CRC32::EncryptData((void*)(message + MESSAGE_HEADER_LENGTH), length);
}

// Message retransmission
#define RETRANSMISSION_LIST Netgame::PacketsToResend
#define RETRANSMISSION_OCCUPIED_AT(packetNum) (RETRANSMISSION_LIST[packetNum].resend.ackNum)

PUBLIC bool Packet::GetAck(int connectionID) {
    NetworkConnection* connection = Netgame::GetConnection(connectionID);
    if (!connection)
        return false;

    int ackNum = connection->ack.sequence;

    for (int i = 0; i < MAX_MESSAGES_TO_RESEND; i++) {
        if (RETRANSMISSION_OCCUPIED_AT(i))
            continue;

        Packet* packet = &(RETRANSMISSION_LIST[i]);
        PacketRetransmission* resend = &packet->resend;

        resend->ackNum = resend->ackSequence = ackNum;
        Ack::GetNext(&connection->ack.sequence);

        resend->sendTo = connectionID;
        resend->sentAt = Clock::GetTicks();
        resend->timesResent = 0;

        message.ack.sequence = message.ack.lastReceived = ackNum;

        packet->message = message;
        packet->messageLength = messageLength;

        return true;
    }

    NETWORK_DEBUG_ERROR("Too many messages to resend");

    Netgame::FatalError();

    return false;
}

PUBLIC STATIC void Packet::ResendMessages() {
    for (int i = 0; i < MAX_MESSAGES_TO_RESEND; i++) {
        if (!RETRANSMISSION_OCCUPIED_AT(i))
            continue;

        Packet* packet = &(RETRANSMISSION_LIST[i]);
        PacketRetransmission* resend = &packet->resend;

        int connectionID = resend->sendTo;
        NetworkConnection* connection = Netgame::GetConnection(connectionID);
        if (!connection)
            continue;

        double time = Clock::GetTicks() - resend->sentAt;
        if (time < MESSAGE_RESEND_TIMEOUT)
            continue;

        Message messageToResend = packet->message;

        if (resend->timesResent >= MAX_MESSAGE_RESEND_ATTEMPTS) {
            NETWORK_DEBUG_ERROR("Attempted to resend %d (ack %d) too many times, closing connection",
                messageToResend.type, resend->ackNum);

            if (Netgame::Server) {
                Packet::ResendListRemove(i);
                Netgame::CloseConnection(i, false);
                continue;
            }
            else {
                Packet::ResendListClear();
                Netgame::Disconnect();
                return;
            }
        }

        messageToResend.ack.lastReceived = Ack::GetLastReceived(connectionID);

        resend->sentAt = Clock::GetTicks();
        resend->ackSequence = connection->ack.sequence;
        resend->timesResent++;

        NETWORK_DEBUG_VERBOSE("Resending %d to %d (resent %d times, ack %d)",
            messageToResend.type, connectionID, resend->timesResent, resend->ackNum);

        Netgame::WriteOutgoingData((Uint8*)(&messageToResend), packet->messageLength);
        Netgame::Send(connectionID);
    }
}

PUBLIC STATIC void Packet::ResendListIterate(int connectionID, int num, void (*func)(int curAck, int ackNum)) {
    for (int i = 0; i < MAX_MESSAGES_TO_RESEND; i++) {
        if (RETRANSMISSION_OCCUPIED_AT(i) && connectionID == RETRANSMISSION_LIST[i].resend.sendTo) {
            if (func)
                func(i, num);
            else
                Packet::ResendListRemove(i);
        }
    }
}

PUBLIC STATIC void Packet::ResendListRemove(int packetNum) {
    if (packetNum >= 0 && packetNum < MAX_MESSAGES_TO_RESEND) {
        Packet* packet = &(RETRANSMISSION_LIST[packetNum]);
        packet->resend.ackNum = 0;
    }
}

PUBLIC STATIC void Packet::ResendListClear() {
    memset(RETRANSMISSION_LIST, 0x00, sizeof(RETRANSMISSION_LIST));
}

PUBLIC STATIC void Packet::RemovePendingPackets(int connectionID) {
    Packet::ResendListIterate(connectionID, 0, NULL);
}

#undef RETRANSMISSION_OCCUPIED_AT

#ifdef BIG_ENDIAN
static Uint16 SwapShort(Uint16 value) {
    return ((value & 0xFF) >> 8) | ((value << 8) & 0xFF);
}

static Uint32 SwapLong(Uint32 value) {
    Uint8 a = (value & 0xFF) << 24;
    Uint8 b = (value & 0xFF00) << 8;
    Uint8 c = (value >> 8) & 0xFF00;
    Uint8 d = (value >> 24) & 0xFF;
    return a | b | c | d;
}
#endif

// Writes data
PUBLIC STATIC void Packet::WriteUint8(Uint8** buffer, Uint8 data) {
    **buffer = data;
    *buffer++;
}

PUBLIC STATIC void Packet::WriteSint8(Uint8** buffer, Sint8 data) {
    **buffer = (Uint8)data;
    *buffer++;
}

PUBLIC STATIC void Packet::WriteUint16(Uint8** buffer, Uint16 data) {
    Uint16* buf = (Uint16*)(*buffer);
#ifdef BIG_ENDIAN
    *buf = SwapShort(data);
#else
    *buf = data;
#endif
    *buffer += 2;
}

PUBLIC STATIC void Packet::WriteSint16(Uint8** buffer, Sint16 data) {
    Sint16* buf = (Sint16*)(*buffer);
#ifdef BIG_ENDIAN
    *buf = SwapShort(data);
#else
    *buf = data;
#endif
    *buffer += 2;
}

PUBLIC STATIC void Packet::WriteUint32(Uint8** buffer, Uint32 data) {
    Uint32* buf = (Uint32*)(*buffer);
#ifdef BIG_ENDIAN
    *buf = SwapLong(data);
#else
    *buf = data;
#endif
    *buffer += 4;
}

PUBLIC STATIC void Packet::WriteSint32(Uint8** buffer, Sint32 data) {
    Sint32* buf = (Sint32*)(*buffer);
#ifdef BIG_ENDIAN
    *buf = SwapLong(data);
#else
    *buf = data;
#endif
    *buffer += 4;
}

PUBLIC STATIC void Packet::Write(Uint8** buffer, Uint8 data) {
    Packet::WriteUint8(buffer, data);
}

PUBLIC STATIC void Packet::Write(Uint8** buffer, Sint8 data) {
    Packet::WriteSint8(buffer, data);
}

PUBLIC STATIC void Packet::Write(Uint8** buffer, Uint16 data) {
    Packet::WriteUint16(buffer, data);
}

PUBLIC STATIC void Packet::Write(Uint8** buffer, Sint16 data) {
    Packet::WriteSint16(buffer, data);
}

PUBLIC STATIC void Packet::Write(Uint8** buffer, Uint32 data) {
    Packet::WriteUint32(buffer, data);
}

PUBLIC STATIC void Packet::Write(Uint8** buffer, Sint32 data) {
    Packet::WriteSint32(buffer, data);
}

// Reads data
PUBLIC STATIC Uint8 Packet::ReadUint8(Uint8** buffer) {
    Uint8 data = **buffer;
    *buffer++;
    return data;
}

PUBLIC STATIC Sint8 Packet::ReadSint8(Uint8** buffer) {
    Sint8 data = (Sint8)(**buffer);
    *buffer++;
    return data;
}

PUBLIC STATIC Uint16 Packet::ReadUint16(Uint8** buffer) {
    Uint16 data = (Uint16)(**buffer);
#ifdef BIG_ENDIAN
    data = SwapShort(data);
#endif
    *buffer += 2;
    return data;
}

PUBLIC STATIC Sint16 Packet::ReadSint16(Uint8** buffer) {
    Sint16 data = (Sint16)(**buffer);
#ifdef BIG_ENDIAN
    data = SwapShort(data);
#endif
    *buffer += 2;
    return data;
}

PUBLIC STATIC Uint32 Packet::ReadUint32(Uint8** buffer) {
    Uint32 data = (Uint32)(**buffer);
#ifdef BIG_ENDIAN
    data = SwapShort(data);
#endif
    *buffer += 4;
    return data;
}

PUBLIC STATIC Sint32 Packet::ReadSint32(Uint8** buffer) {
    Sint32 data = (Sint32)(**buffer);
#ifdef BIG_ENDIAN
    data = SwapShort(data);
#endif
    *buffer += 4;
    return data;
}

PUBLIC STATIC Uint8 Packet::Read(Uint8** buffer) {
    return Packet::ReadUint8(buffer);
}

#undef READ_FUNC

// Array manipulation
PUBLIC STATIC void Packet::WriteArray(Uint8** buffer, Uint8* data, size_t length) {
    while (length--) {
        **buffer = *data++;
        *buffer++;
    }
}

PUBLIC STATIC void Packet::ReadArray(Uint8** buffer, Uint8* data, size_t length) {
    while (length--) {
        *data++ = **buffer;
        *buffer++;
    }
}
