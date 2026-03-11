#ifndef CLIENT_H
#define CLIENT_H

#include "buffer.h"
#include "request_parser.h"
#include "ring.h"

#include <time.h>
#include <poll.h>
#include <sys/socket.h>

struct Client
{
    struct RequestParser parser;            /* HTTP parser */
    size_t response_size;                   /* Remaining response size after which the new response size should be read from output_buffer */
    struct Ring input_buffer;               /* Messages to be processed */
    struct Ring output_buffer;              /* Messages to be sent later */
    struct sockaddr_storage address;        /* Client's address */
    struct timespec last_input_complete;    /* Last time when the input buffer was empty or a complete request was received */
    struct timespec last_input_nonempty;    /* Last time when the input buffer was not empty */
    struct timespec last_output_not_full;   /* Last time when the output buffer was empty */
    struct timespec last_output_complete;   /* Last time when the input buffer was empty or a complete response was sent */
    unsigned int shuffle_index;             /* Index storage for shuffling, unrelated to the client */
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
