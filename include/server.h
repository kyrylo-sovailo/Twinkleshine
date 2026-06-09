#ifndef SERVER_H
#define SERVER_H

#include "../commonlib/include/bool.h"
#include "../commonlib/include/macro.h"
#include "../include/extended_error.h"

#include <sys/socket.h>

#include <stddef.h>
#include <time.h>

struct Client;
struct ClientBuffer;
struct Value;
struct PollBuffer;
struct Ring;

enum ConnectionFlag
{
    CF_NO           = 0,
    CF_SATURATED    = 1,    /* send() failed */
    CF_EXHAUSTED    = 2,    /* read() failed */
    CF_CLOSED       = 4     /* read() failed because of closed connection */
};

/* initialize.c */
struct Error *server_initialize(struct PollBuffer *polls) NODISCARD;
void server_finalize(struct ClientBuffer *clients, struct PollBuffer *polls);

/* low_level.c */
/* Sends the given value (Failure -> close and log) */
struct Error *server_send_value(struct Value *value, int fd, enum ConnectionFlag *flags) NODISCARD;
/* Sends the message */
struct ExError server_send_message(struct Value *value, int fd, const struct sockaddr *address, unsigned int first_chunk, struct Value *chunks, size_t last_chunk_size) NODISCARD;
/* Receives the given amount of bytes to ring */
struct ExError server_receive_value(struct Ring *ring, int fd, size_t size, enum ConnectionFlag *flags) NODISCARD;

/* accept.c */
/* Processes accepting sockets and creates new clients (Failure -> die) */
struct Error *server_accept_traffic(struct ClientBuffer *clients, struct PollBuffer *polls, double utilization, time_t now) NODISCARD;

/* receive.c */
/* Allocates client's input buffer and reads MIN_AVAILABLE_REQUEST_STREAM to MAX_REQUEST_STREAM_SIZE bytes of data into it */
struct ExError server_receive_data(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags) NODISCARD;

/* process.c */
/* Parses the data in the input buffer, and if the request is complete, processes it and pushes into output buffer */
struct ExError server_process_data(struct Client *client, time_t now) NODISCARD;

/* send.c */
/* Sends the client's output buffer */
struct ExError server_send_data(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags) NODISCARD;

/* high_level.c */
/* Runs the server (Failure -> die) */
struct Error *server_main(void) NODISCARD;

#endif
