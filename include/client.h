#ifndef CLIENT_H
#define CLIENT_H

#include "buffer.h"
#include "request_parser.h"

#include <poll.h>

struct Client
{
    struct RequestParser parser;
    bool active;
};

DECLARE_BUFFER(struct Client, ClientBuffer)
DECLARE_BUFFER_RESIZE(struct Client, ClientBuffer, clients_)
DECLARE_BUFFER_FINALIZE(struct Client, ClientBuffer, clients_)

DECLARE_BUFFER(struct pollfd, PollBuffer)
DECLARE_BUFFER_RESIZE(struct pollfd, PollBuffer, polls_)
DECLARE_BUFFER_FINALIZE(struct pollfd, PollBuffer, polls_)

#endif
