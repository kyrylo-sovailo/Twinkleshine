#ifndef CLIENT_H
#define CLIENT_H

#include "parser.h"
#include "processor.h"
#include "ring.h"
#include "../commonlib/include/buffer.h"

#include <poll.h>
#include <sys/socket.h>
#include <time.h>

enum ClientType
{
    CT_HTTP,
    CT_GOPHER,
    CT_FINGER,
    CT_GEMINI,
    CT_NEX
};

enum CryptographyState
{
    CS_PENDING_HANDSHAKE = 0,   /* Handshake in progress */
    CS_OPERATIONAL,             /* Handshake finished */
    CS_PENDING_SHUTDOWN,        /* Shutdown in progress */
    CS_SHUTDOWN                 /* Shutdown finished */
};

struct Client
{
    enum ClientType type;

    /* Input */
    struct Ring request_stream;                 /* Buffer for incoming data, contains many request contents */
    struct Request request;                     /* Metadata of the first request in request_stream */
    struct Parser parser;                       /* request and parser together form a state machine for parsing first request in request_stream */    

    /* Output */
    struct Ring response_stream;                /* Buffer for outgoing data, contains response contents */
    struct Response response;                   /* Metadata of the first response in response_stream */
    struct Ring response_queue;                 /* Metadata of all other responses in response_stream */
    size_t response_count;                      /* Metadata count */

    /* Time keeping */
    time_t last_request_complete;               /* Last time when the input buffer was empty or a complete request was received */
    time_t last_request_stream_not_empty;       /* Last time when the input buffer was not empty */
    time_t last_response_stream_not_full;       /* Last time when the output buffer was not full */
    time_t last_response_complete;              /* Last time when the input buffer was empty or a complete response was sent */

    /* Cryptography */
    enum CryptographyState cryptography_state;  /* State */
    void *ssl, *read_bio, *write_bio;           /* OpenSSL stuff */
    
    /* Other */
    struct sockaddr_storage address;            /* Client's IP address */
    size_t shuffle_index;                       /* Index storage for shuffling, unrelated to the client */
};

DECLARE_BUFFER(struct Client, ClientBuffer)
DECLARE_BUFFER_APPEND(struct Client, ClientBuffer, clients_)
DECLARE_BUFFER_FINALIZE(struct Client, ClientBuffer, clients_)

DECLARE_BUFFER(struct pollfd, PollBuffer)
DECLARE_BUFFER_APPEND(struct pollfd, PollBuffer, polls_)
DECLARE_BUFFER_FINALIZE(struct pollfd, PollBuffer, polls_)

extern struct ConstValue g_short_response_stream;       /* Short-term buffer that replaces some client's response_stream */
extern struct Client *g_short_response_stream_owner;    /* Which client does g_short_response_stream belong to */

/* Sets the value shuffle index of N first clients to a random unique value */
void clients_shuffle(struct ClientBuffer *clients);

/* Removes dead clients  */
void clients_remove_finalized(struct ClientBuffer *clients, struct PollBuffer *polls);

/* Finalizes client */
void client_finalize(struct Client *client);

/* Finalizes poll */
void poll_finalize(struct pollfd *poll);

/* Returns the size of the response stream (including the buffer) */
size_t client_response_stream_size(const struct Client *client);

#endif
