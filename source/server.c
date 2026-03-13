#include "../include/server.h"
#include "../include/client.h"
#include "../include/constants.h"
#include "../include/output.h"
#include "../include/path.h"
#include "../include/request_parser.h"
#include "../include/request_processor.h"
#include "../include/ring.h"
#include "../include/tables.h"

#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

enum ErrorFlags
{
    ERF_FATAL   = 1 << ERROR_TYPE_BITS, /* Total failure */
    ERF_CLOSE   = 2 << ERROR_TYPE_BITS, /* Close the connection */
    ERF_MESSAGE = 4 << ERROR_TYPE_BITS, /* Message the client before closing connection */
    ERF_LOG     = 8 << ERROR_TYPE_BITS  /* Log the incident */
};

struct ErrorAction
{
    struct Error *error;    /* Error trace */
    int flags;              /* What do do with that error trace BITWISE OR enum ErrorType */
};

/* Creates listening sockets and puts them into polls */
static struct Error *create_listening_sockets(struct PollBuffer *polls) NODISCARD;
static struct Error *create_listening_sockets(struct PollBuffer *polls)
{
    struct Error *error;
    struct pollfd new_poll = { -1, POLLIN, 0 };
    struct sockaddr_in socket_address = { 0 };

    new_poll.fd = socket(AF_INET, SOCK_STREAM, 0);
    AGOTO0(new_poll.fd >= 0, "socket() failed"); /* Failure -> fatal */

    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(8000);
    AGOTO0(bind(new_poll.fd, (struct sockaddr*)&socket_address, sizeof(socket_address)) >= 0, "bind() failed"); /* Failure -> fatal */
    AGOTO0(listen(new_poll.fd, 10) >= 0, "listed() failed"); /* Failure -> fatal */
    PGOTO(polls_append(polls, &new_poll, 1)); /* Failure -> fatal */
    return OK;

    failure:
    if (new_poll.fd >= 0) close(new_poll.fd);
    return error;
}

/* Accepts new connections and puts them into clients */
static struct Error *process_listening_sockets(struct ClientBuffer *clients, struct PollBuffer *polls) NODISCARD;
static struct Error *process_listening_sockets(struct ClientBuffer *clients, struct PollBuffer *polls)
{
    struct Error *error;
    struct pollfd new_poll = { -1, POLLIN | POLLHUP | POLLERR, 0 };
    struct Client new_client = { 0 };
    socklen_t socket_address_size = sizeof(new_client.address);
    const size_t old_clients_size = clients->size;

    
    AGOTO0((polls->p[0].revents & (POLLERR | POLLHUP)) == 0, "listening socket failed"); /* Failure -> fatal */
    if ((polls->p[0].revents & POLLIN) == 0) return OK;

    new_poll.fd = accept(polls->p[0].fd, (struct sockaddr*)&new_client.address, &socket_address_size);
    AGOTO0(new_poll.fd >= 0, "accept() failed"); /* Failure -> fatal */
    AGOTO0(fcntl(new_poll.fd, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed"); /* Failure -> fatal */
    PGOTO(clients_append(clients, &new_client, 1)); /* Failure -> fatal */
    PGOTO(polls_append(polls, &new_poll, 1)); /* Failure -> fatal */
    return OK;

    failure:
    clients->size = old_clients_size;
    if (new_poll.fd >= 0) close(new_poll.fd);
    return error;
}

/* Set's the value shuffle index of N first clients to a random unique value */
static void shuffle_clients(struct Client *clients, size_t size)
{
    /* Fisher-Yates shuffle */
    size_t i;
    for (i = 0; i < size; i++) clients[i].shuffle_index = i;
    if (size == 0) return;
    for (i = size - 1; i >= 1; i--)
    {
        size_t j = (unsigned int)rand() % (i + 1);
        size_t buffer = clients[i].shuffle_index;
        clients[i].shuffle_index = clients[j].shuffle_index;
        clients[j].shuffle_index = buffer;
    }
}

/* Allocates client's input buffer and reads data into it, expects EPOLLIN */
struct ErrorAction client_receive_data(struct Client *client, const struct pollfd *poll) NODISCARD;
struct ErrorAction client_receive_data(struct Client *client, const struct pollfd *poll)
{
    struct ErrorAction action = { OK, ERF_CLOSE | ERF_LOG };
    struct Error *error;
    int signed_available; size_t available;
    size_t new_input_buffer_size;
    struct ValueLocation location; struct Value value;
    unsigned char i;

    /* Get how much data is available */
    AGOTO0(ioctl(poll->fd, FIONREAD, &signed_available) >= 0, "ioctl() failed"); /* Failure -> close */
    AGOTO0(signed_available >= 0, "ioctl() failed"); /* Failure -> close */
    action.flags |=  (ERR_TOO_FAST | ERF_MESSAGE);
    AGOTO0(signed_available <= MAX_AVAILABLE_INPUT, "ioctl() failed"); /* Failure -> send ERR_TOO_FAST and close */
    action.flags &= ~(ERR_TOO_FAST | ERF_MESSAGE);
    available = (size_t)signed_available;

    /* Allocate input buffer */
    new_input_buffer_size = client->input_buffer.size + available;
    if (new_input_buffer_size > MAX_INPUT_BUFFER_SIZE) new_input_buffer_size = MAX_INPUT_BUFFER_SIZE;
    PGOTO(ring_reserve(&client->input_buffer, new_input_buffer_size)); /* Failure -> close */

    /* Read data into buffer */
    location.offset = 0;
    location.size = new_input_buffer_size - client->input_buffer.size;
    action.flags |=  ERF_FATAL;
    PGOTO(ring_push(&client->input_buffer, location.size)); /* Failure -> fatal */
    PGOTO(ring_get(&client->input_buffer, &location, true, &value)); /* Failure -> fatal */
    action.flags &= ~ERF_FATAL;
    for (i = 0; i < 2; i++)
    {
        ssize_t signed_received; size_t received;
        signed_received = read(poll->fd, value.parts[i].p, value.parts[i].size);
        AGOTO0(signed_received >= 0, "read() failed"); /* Failure -> close */
        received = (size_t)signed_received;
        AGOTO0(received == value.parts[i].size, "read() failed"); /* Failure -> close */
    }
    error = OK;

    failure:
    action.error = error;
    return action;
}

/* Parse the data in the input buffer, try to sent it, and put in the output buffer if failed */
struct ErrorAction client_parse_data(struct Client *client, const struct pollfd *poll) NODISCARD;
struct ErrorAction client_parse_data(struct Client *client, const struct pollfd *poll)
{
    struct ErrorAction action = { OK, ERF_CLOSE | ERF_LOG };
    struct Error *error;

    while (true)
    {
        struct ValuePart response;
        size_t new_output_buffer_size;
        struct ValueLocation location; struct Value value;
        unsigned char i;

        /* Parse request */
        action.flags |=  (ERR_PARSING_FAILED | ERF_MESSAGE);
        PGOTO(request_parser_parse(&client->parser, &client->input_buffer)); /* Failure -> send ERR_TOO_LARGE/ERR_PARSING_FAILED and close */
        action.flags &= ~(ERR_PARSING_FAILED | ERF_MESSAGE);
        if (client->parser.state != RPS_END) break;

        /* Generate response */
        action.flags |=  (ERR_UNKNOWN | ERF_MESSAGE);
        PGOTO(request_processor_process(&client->input_buffer, &client->parser.request, &response)); /* Failure -> send ERR_UNKNOWN and close */
        action.flags &= ~(ERR_UNKNOWN | ERF_MESSAGE);

        /* Try to send response immediately, bypassing output buffer */
        if (client->output_buffer.size == 0)
        {
            ssize_t signed_sent;
            size_t sent;
            signed_sent = send(poll->fd, response.p, response.size, 0);
            AGOTO0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed"); /* Failure -> close */
            sent = (signed_sent >= 0) ? (size_t)signed_sent : 0;
            response.p += sent;
            response.size -= sent;
        }
        if (response.size == 0) continue;

        /* Relocate the rest to the buffer (this is the only place where memcpy takes place, apart from buffer and ring resizes) */
        new_output_buffer_size = client->output_buffer.size + response.size; /* Do not cap, output gets written anyway */
        PGOTO(ring_reserve(&client->output_buffer, new_output_buffer_size)); /* Failure -> close */
        location.offset = 0;
        location.size = new_output_buffer_size - client->output_buffer.size;
        action.flags |=  ERF_FATAL;
        PGOTO(ring_push(&client->output_buffer, location.size)); /* Failure -> fatal */
        PGOTO(ring_get(&client->output_buffer, &location, true, &value)); /* Failure -> fatal */
        action.flags &= ~ERF_FATAL;
        for (i = 0; i < 2; i++)
        {
            memcpy(value.parts[i].p, response.p, value.parts[i].size);
            response.p += value.parts[i].size;
            response.size -= value.parts[i].size;
        }
    }
    error = OK;

    failure:
    action.error = error;
    return action;
}

/* Send the output buffer */
struct ErrorAction client_send_data(struct Client *client, struct pollfd *poll) NODISCARD;
struct ErrorAction client_send_data(struct Client *client, struct pollfd *poll)
{
    struct ErrorAction action = { OK, ERF_CLOSE | ERF_LOG };
    struct Error *error;
    struct Value location;
    size_t total_sent = 0;
    unsigned char i;

    ring_get_all(&client->output_buffer, &location);
    for (i = 0; i < 2; i++)
    {
        ssize_t signed_sent; size_t sent;
        signed_sent = send(poll->fd, location.parts[i].p, location.parts[i].size, 0);
        AGOTO0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed"); /* Failure -> close */
        sent = (signed_sent >= 0) ? (size_t)signed_sent : 0;
        total_sent += sent;
        if (sent != location.parts[i].size) break;
    }
    action.flags |=  ERF_FATAL;
    PGOTO(ring_pop(&client->output_buffer, total_sent)); /* Failure -> fatal */
    action.flags &= ~ERF_FATAL;
    error = OK;

    failure:
    action.error = error;
    return action;
}

/* Processes client, call when revents has something to it */
struct Error *client_process(struct Client *client, struct pollfd *poll) NODISCARD;
struct Error *client_process(struct Client *client, struct pollfd *poll)
{
    struct ErrorAction action = { OK, ERF_CLOSE | ERF_LOG };
    struct Error *error;

    /* Check if connection failed */
    AGOTO0(((poll->revents & (POLLERR | POLLHUP)) == 0), "Connection failed"); /* Failure -> close */

    /* Do everything you can */
    while (true)
    {
        action = client_receive_data(client, poll);
        PGOTO(action.error);
        action = client_parse_data(client, poll);
        PGOTO(action.error);
        action = client_send_data(client, poll);
        PGOTO(action.error);
    }
    error = OK;

    failure:
    action.error = error;
    if ((action.flags & ERF_LOG) != 0)
    {
        error_print(action.error);
    }
    if ((action.flags & ERF_MESSAGE) != 0)
    {
        struct ValuePart response;
        request_processor_error(action.flags & ((1 << ERROR_TYPE_BITS) - 1), &response);
        (void)send(poll->fd, response.p, response.size, 0);
    }
    if ((action.flags & ERF_CLOSE) != 0)
    {
        ring_finalize(&client->input_buffer);
        ring_finalize(&client->output_buffer);
        close(poll->fd);
        poll->fd = -1;
    }
    if ((action.flags & ERF_FATAL) != 0)
    {
        return action.error;
    }
    return OK;
}

/* Processes all clients */
static struct Error *clients_process(struct ClientBuffer *clients, struct PollBuffer *polls) NODISCARD;
static struct Error *clients_process(struct ClientBuffer *clients, struct PollBuffer *polls)
{
    int event_number;
    struct Client *client_i, *surviving_client_i;
    struct pollfd *poll_i, *surviving_poll_i;

    /* Wait for events */
    event_number = poll(polls->p, polls->size, -1);
    ARET0(event_number >= 0, "poll() failed"); /* Failure -> fatal */

    /* Shuffle events */
    shuffle_clients(clients->p, clients->size);

    /* Accept new connections */
    PRET(process_listening_sockets(clients, polls));

    /* Process events */
    for (client_i = clients->p; client_i < clients->p + clients->size; client_i++)
    {
        const size_t shuffle_index = client_i->shuffle_index;
        PRET(client_process(clients->p + shuffle_index, polls->p + shuffle_index + 1));
    }

    /* Delete dead clients */
    for (surviving_client_i = client_i = clients->p, surviving_poll_i = poll_i = polls->p + 1; client_i < clients->p + clients->size; client_i++, poll_i++)
    {
        if (poll_i->fd >= 0)
        {
            if (surviving_client_i != client_i)
            {
                *surviving_client_i = *client_i;
                *surviving_poll_i = *poll_i;
            }
            surviving_client_i++;
            surviving_poll_i++;
        }
    }
    clients->size -= (size_t)(surviving_client_i - clients->p);
    polls->size -= (size_t)(surviving_poll_i - polls->p);
    return OK;
}

struct Error *server_main(int argc, char **argv)
{
    struct Error *error;
    struct ClientBuffer clients = { 0 };
    struct PollBuffer polls = { 0 };

    output_initialize();
    tables_initialize();
    AGOTO0(argc >= 0, "Not enough arguments");
    PGOTO(path_set_application(&g_application, argv[0]));

    /* Create socket */
    PGOTO(create_listening_sockets(&polls));
    
    /* Loop */
    while (true)
    {
        PGOTO(clients_process(&clients, &polls));
    }
    error = OK;

    failure:
    /* TODO: finalize */
    return error;
}

