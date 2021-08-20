#ifndef ENGINE_NETWORK_TCPSOCKET_H
#define ENGINE_NETWORK_TCPSOCKET_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Network/Socket.h>
#include <Engine/Network/SocketIncludes.h>

class TCPSocket : public Socket {
public:
    static TCPSocket* New(Uint16 port, int protocol);
    static TCPSocket* Open(Uint16 port, int protocol);
    static TCPSocket* OpenClient(const char* address, Uint16 port, int protocol);
    static TCPSocket* OpenServer(Uint16 port, int protocol);
    bool Bind(Uint16 port, int family);
    bool Listen();
    TCPSocket* Accept();
    int Send(Uint8* data, size_t length);
    int Send(Uint8* data, size_t length, sockaddr_storage* addrStorage);
    int Receive(Uint8* data, size_t length);
    int Receive(Uint8* data, size_t length, sockaddr_storage* addrStorage);
};

#endif /* ENGINE_NETWORK_TCPSOCKET_H */
