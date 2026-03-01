#ifndef CLIENT_H
#define CLIENT_H

#include "buffer.h"
#include "request_parser.h"

#include <poll.h>
#include <sys/socket.h>

struct Client
{
    struct RequestParser parser;
    struct sockaddr_storage address;
};

DECLARE_BUFFER(struct Client, ClientBuffer)
DECLARE_BUFFER_RESIZE(struct Client, ClientBuffer, clients_)
DECLARE_BUFFER_APPEND(struct Client, ClientBuffer, clients_)
DECLARE_BUFFER_FINALIZE(struct Client, ClientBuffer, clients_)

DECLARE_BUFFER(struct pollfd, PollBuffer)
DECLARE_BUFFER_RESIZE(struct pollfd, PollBuffer, polls_)
DECLARE_BUFFER_APPEND(struct pollfd, PollBuffer, polls_)
DECLARE_BUFFER_FINALIZE(struct pollfd, PollBuffer, polls_)

#endif
