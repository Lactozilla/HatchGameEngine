#ifndef ENGINE_NETWORK_PACKET_H
#define ENGINE_NETWORK_PACKET_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED



class Packet {
private:
    static Uint32 ChecksumPayload(Uint8* message, size_t length);

public:
    Message message;
    int messageLength;
    PacketRetransmission resend;

    void Init();
    bool PrepareMessage(int connection, size_t length, bool useAck);
    bool Send(int connection, size_t length, bool useAck);
    static bool Process(Message* outMessage);
    void Checksum();
    bool GetAck(int connectionID);
    static void ResendMessages();
    static void ResendListIterate(int connectionID, int num, void (*func)(int curAck, int ackNum));
    static void ResendListRemove(int packetNum);
    static void ResendListClear();
    static void RemovePendingPackets(int connectionID);
    static Uint16 SwapShort(Uint16 value);
    static Uint32 SwapLong(Uint32 value);
    static void WriteUint8(Uint8** buffer, Uint8 data);
    static void WriteSint8(Uint8** buffer, Sint8 data);
    static void WriteUint16(Uint8** buffer, Uint16 data);
    static void WriteSint16(Uint8** buffer, Sint16 data);
    static void WriteUint32(Uint8** buffer, Uint32 data);
    static void WriteSint32(Uint8** buffer, Sint32 data);
    static void Write(Uint8** buffer, Uint8 data);
    static void Write(Uint8** buffer, Sint8 data);
    static void Write(Uint8** buffer, Uint16 data);
    static void Write(Uint8** buffer, Sint16 data);
    static void Write(Uint8** buffer, Uint32 data);
    static void Write(Uint8** buffer, Sint32 data);
    static Uint8 ReadUint8(Uint8** buffer);
    static Sint8 ReadSint8(Uint8** buffer);
    static Uint16 ReadUint16(Uint8** buffer);
    static Sint16 ReadSint16(Uint8** buffer);
    static Uint32 ReadUint32(Uint8** buffer);
    static Sint32 ReadSint32(Uint8** buffer);
    static Uint8 Read(Uint8** buffer);
    static void WriteArray(Uint8** buffer, Uint8* data, size_t length);
    static void ReadArray(Uint8** buffer, Uint8* data, size_t length);
};

#endif /* ENGINE_NETWORK_PACKET_H */
