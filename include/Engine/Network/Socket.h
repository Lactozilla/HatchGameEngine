#ifndef ENGINE_NETWORK_SOCKET_H
#define ENGINE_NETWORK_SOCKET_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Network/SocketIncludes.h>

class Socket {
private:
    static sockaddr_storage* AllocSockAddr(sockaddr_storage* sa);
    static char* FormatAddress(int family, void* addr, int port);

public:
    int protocol;
    int ipProtocol, family;
    SocketAddress address;
    bool server;
    socket_t sock;
    int error;

    static bool IsProtocolValid(int protocol);
    static int GetFamilyFromProtocol(int protocol);
    static int GetDomainFromProtocol(int protocol);
    static int GetProtocolFromFamily(int family);
    static char* GetProtocolName(int protocol);
    static char* GetAnyAddress(int protocol);
    bool AddressToSockAddr(SocketAddress* sockAddress, int protocol, sockaddr_storage* sa);
    int SockAddrToAddress(SocketAddress* sockAddress, int family, sockaddr_storage* sa);
    void MakeSockAddr(int family, sockaddr_storage* sa);
    bool MakeHints(addrinfo* hints, int type, int protocol);
    bool GetAddressFromSockAddr(int family, sockaddr_storage* sa);
    static char* PortToString(Uint16 port);
    sockaddr_storage* ResolveAddrInfo(const char* hostIn, Uint16 portIn, addrinfo* hints);
    sockaddr_storage* ResolveHost(const char* hostIn, Uint16 portIn);
    sockaddr_storage* ResolveHost(const char* hostIn, Uint16 portIn, int type, int ipProto);
    static bool CompareAddresses(SocketAddress* addrA, SocketAddress* addrB);
    static bool CompareSockAddr(sockaddr_storage* addrA, sockaddr_storage* addrB);
    static char* AddressToString(int protocol, SocketAddress* sockAddr);
    static char* SockAddrToString(int family, sockaddr_storage* addrStorage);
    void SetNoDelay();
    bool SetBlocking();
    bool SetNonBlocking();
    void Close();
    static Socket* New(Uint16 port, int protocol);
    static Socket* Open(Uint16 port, int protocol);
    static Socket* OpenServer(Uint16 port, int protocol);
    static Socket* OpenClient(const char* address, Uint16 port, int protocol);
    virtual bool Connect(const char* address, Uint16 port);
    virtual bool Bind(sockaddr_storage* addrStorage);
    virtual bool Bind(Uint16 port, int family);
    virtual bool Listen();
    virtual Socket* Accept();
    virtual int Send(Uint8* data, size_t length);
    virtual int Send(Uint8* data, size_t length, sockaddr_storage* addrStorage);
    virtual int Receive(Uint8* data, size_t length);
    virtual int Receive(Uint8* data, size_t length, sockaddr_storage* addrStorage);
};

#endif /* ENGINE_NETWORK_SOCKET_H */
