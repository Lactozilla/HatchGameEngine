#ifndef ENGINE_NETWORK_UDPSOCKET_H
#define ENGINE_NETWORK_UDPSOCKET_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>

class UDPSocket : public Socket {
public:
    static UDPSocket* New(Uint16 port, int protocol);
    static UDPSocket* Open(Uint16 port, int protocol);
    static UDPSocket* OpenClient(const char* address, Uint16 port, int protocol);
    static UDPSocket* OpenServer(Uint16 port, int protocol);
    bool Bind(sockaddr_storage* addrStorage);
    bool Connect(const char* address, Uint16 port);
    int Send(Uint8* data, size_t length, sockaddr_storage* addrStorage);
    int Receive(Uint8* data, size_t length, sockaddr_storage* addrStorage);
};

#endif /* ENGINE_NETWORK_UDPSOCKET_H */
