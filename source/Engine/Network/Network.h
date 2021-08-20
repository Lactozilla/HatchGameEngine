#ifndef NETWORK_H
#define NETWORK_H

enum {
    NETGAME_UDP,
    NETGAME_TCP
};

#define NETWORK_DEBUG

#ifdef NETWORK_DEBUG

#include <Engine/Diagnostics/Log.h>

#define NETWORK_DEBUG_INFO(...) \
    Log::Print(Log::LOG_INFO, "[" __FUNCTION__ "] " __VA_ARGS__)
#define NETWORK_DEBUG_ERROR(...) \
    Log::Print(Log::LOG_ERROR, "[" __FUNCTION__ "] " __VA_ARGS__)

#else

#define NETWORK_DEBUG_INFO
#define NETWORK_DEBUG_ERROR

#endif

#endif /* NETWORK_H */
