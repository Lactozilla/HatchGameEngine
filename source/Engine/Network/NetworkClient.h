#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <Engine/Includes/Standard.h>
#include <Engine/Network/SocketIncludes.h>

struct NetworkClient {
    int socket;
    SocketAddress address;
};

#endif /* NETWORKCLIENT_H */
