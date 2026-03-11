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

struct Server
{
    struct ClientBuffer clients;
    struct PollBuffer polls;
};

/* Creates listening sockets and puts them into polls */
static struct Error *create_listening_sockets(struct PollBuffer *polls) NODISCARD;
static struct Error *create_listening_sockets(struct PollBuffer *polls)
{
    struct Error *error = OK;
    struct pollfd new_poll = { -1, POLLIN, 0 };
    struct sockaddr_in socket_address = { 0 };

    new_poll.fd = socket(AF_INET, SOCK_STREAM, 0);
    AGOTO0(new_poll.fd >= 0, "socket() failed");

    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(8000);
    AGOTO0(bind(new_poll.fd, (struct sockaddr*)&socket_address, sizeof(socket_address)) >= 0, "bind() failed");
    AGOTO0(listen(new_poll.fd, 10) >= 0, "listed() failed");
    PGOTO(polls_append(polls, &new_poll, 1));
    return OK;

    failure:
    if (new_poll.fd >= 0) close(new_poll.fd);
    return error;
}

/* Accepts new connections and puts them into clients */
static struct Error *accept_new_connections(struct ClientBuffer *clients, struct PollBuffer *polls) NODISCARD;
static struct Error *accept_new_connections(struct ClientBuffer *clients, struct PollBuffer *polls)
{
    struct Error *error = OK;
    struct Client new_client = { 0 };
    struct pollfd new_poll = { -1, POLLIN | POLLHUP | POLLERR, 0 };
    socklen_t socket_address_size;

    if ((polls->p[0].revents & POLLIN) == 0) return OK;

    socket_address_size = sizeof(new_client.address);
    new_poll.fd = accept(polls->p[0].fd, (struct sockaddr*)&new_client.address, &socket_address_size);
    AGOTO0(new_poll.fd >= 0, "accept() failed"); /* TODO: not fatal */
    AGOTO0(fcntl(new_poll.fd, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed"); /* TODO: not fatal */
    PGOTO(clients_append(clients, &new_client, 1));
    PGOTO(polls_append(polls, &new_poll, 1));
    return OK;

    failure:
    if (new_poll.fd >= 0) close(new_poll.fd);
    return error;
}

/* Set's the value shuffle index of N first clients to a random unique value */
static void shuffle_clients(struct ClientBuffer *clients, size_t active_number)
{
    /* Fisher-Yates shuffle */
    size_t i;
    if (active_number == 0) return;
    for (i = active_number - 1; i >= 1; i--)
    {
        size_t j = rand() % (i + 1);
        unsigned int buffer = clients->p[i].shuffle_index;
        clients->p[i].shuffle_index = clients->p[j].shuffle_index;
        clients->p[j].shuffle_index = buffer;
    }
}

/* Processes client, call when revents has something to it */
struct Error *client_process(struct Client *client, struct pollfd *poll, unsigned int timeout) NODISCARD;
struct Error *client_process(struct Client *client, struct pollfd *poll, unsigned int timeout)
{
    struct Error *error;
    int signed_available; size_t available;
    ssize_t signed_received; size_t received;
    size_t new_input_buffer_size;
    struct ValueLocation location;
    struct Value value;
    unsigned char i;

    size_t temp;

    /* Reading data */
    AGOTO0(ioctl(poll->fd, FIONREAD, &signed_available) >= 0, "ioctl() failed"); /* Failure -> close */
    AGOTO0(signed_available >= 0, "ioctl() failed"); /* Failure -> close */
    AGOTO0(signed_available <= MAX_AVAILABLE_INPUT, "ioctl() failed"); /* Failure -> send ERR_TOO_FAST and close */
    available = signed_available;
    new_input_buffer_size = client->input_buffer.size + available;
    if (new_input_buffer_size > MAX_INPUT_BUFFER_SIZE) new_input_buffer_size = MAX_INPUT_BUFFER_SIZE;
    AGOTO(ring_reserve(&client->input_buffer, new_input_buffer_size)); /* Failure -> close */
    location.offset = 0;
    location.size = new_input_buffer_size - client->input_buffer.size;
    AGOTO(ring_push(&client->input_buffer, location.size)); /* Failure -> fatal */
    AGOTO(ring_get(&client->input_buffer, &location, true, &value)); /* Failure -> fatal */
    for (i = 0; i < 2; i++)
    {
        signed_received = read(poll->fd, value.parts[i].p, value.parts[i].size);
        AGOTO0(signed_received >= 0, "read() failed"); /* Failure -> close (???) */
        received = signed_received;
        AGOTO0(received == value.parts[i].size, "read() failed"); /* Failure -> close (???) */
    }

    /* Processing data */
    while (true)
    {
        struct ValuePart response;
        size_t new_output_buffer_size;

        /* Parse request */
        PGOTO(request_parser_parse(&client->parser, &client->input_buffer)); /* Failure -> send ERR_TOO_LARGE/ERR_PARSING_FAILED and close */
        if (client->parser.state != RPS_END) break;

        /* Generate response */
        PGOTO(request_processor_process(&client->input_buffer, &client->parser.request, &response)); /* Failure -> send ERR_UNKNOWN and close */

        /* Try to send response immediately, bypassing output buffer */
        if (client->output_buffer.size == 0)
        {
            ssize_t signed_sent;
            size_t sent;
            signed_sent = send(poll->fd, response.p, response.size, 0);
            AGOTO0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed"); /* Failure -> close */
            sent = (signed_sent >= 0) ? signed_sent : 0;
            response.p += sent;
            response.size -= sent;
        }

        /* Relocate the rest to the buffer (TODO: this is the only place where memcpy takes place, apart from buffer and ring resizes) */
        if (response.size > 0)
        {
            new_output_buffer_size = client->output_buffer.size + response.size; /* Do not cap, output gets written anyway */
            AGOTO(ring_reserve(&client->output_buffer, new_output_buffer_size)); /* Failure -> close */
            location.offset = 0;
            location.size = new_output_buffer_size - client->output_buffer.size;
            AGOTO(ring_push(&client->output_buffer, location.size)); /* Failure -> fatal */
            AGOTO(ring_get(&client->output_buffer, &location, true, &value)); /* Failure -> fatal */
            for (i = 0; i < 2; i++)
            {
                memcpy(value.parts[i].p, response.p, value.parts[i].size);
                response.p += value.parts[i].size;
                response.size -= value.parts[i].size;
            }
        }
    }

    /* Send data from buffer */
    ring_get_all(&client->output_buffer, &value);
    temp = 0;
    for (i = 0; i < 2; i++)
    {
        ssize_t signed_sent;
        size_t sent;
        signed_sent = send(poll->fd, value.parts[i].p, value.parts[i].size, 0);
        AGOTO0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed"); /* Failure -> close */
        sent = (signed_sent >= 0) ? signed_sent : 0;
        temp += sent;
        if (sent != value.parts[i].size) break;
    }
    AGOTO(ring_pop(&client->output_buffer, temp)); /* Failure -> fatal */

    failure:
}

struct Error *server_main(int argc, char **argv)
{
    struct Server server = { 0 };
    struct Error *error = OK;

    struct Client *client_i, *surviving_client_i;
    struct pollfd *poll_i, *surviving_poll_i;

    output_initialize();
    tables_initialize();
    AGOTO0(argc >= 0, "Not enough arguments");
    PGOTO(path_set_application(&g_application, argv[0]));

    /* Create socket */
    PGOTO(create_listening_sockets(&server.polls));
    
    /* Loop */
    while (true)
    {
        /* Waiting */
        int event_number = poll(polls.p, polls.size, -1);
        AGOTO0(event_number >= 0, "poll() failed");

        /*Accepting connections */
        PGOTO(accept_new_connections(&clients, &polls));

        /* Processing connections */
        for (surviving_client_i = client_i = clients.p, surviving_poll_i = poll_i = polls.p + 1; client_i < clients.p + clients.size; client_i++, poll_i++)
        {
            bool delete = false;
            if ((poll_i->revents & POLLIN) != 0)
            {

            }
            else if ((poll_i->revents & POLLHUP) != 0 || (poll_i->revents & POLLERR) != 0)
            {
                /* Delete client */
                delete = true;
            }

            if (delete)
            {
                /* Deleting client */
                char_buffer_finalize(&client_i->parser.request.data);
                close(poll_i->fd);
            }
            else
            {
                /* Relocating client to its new place */
                if (surviving_client_i != client_i)
                {
                    *surviving_client_i = *client_i;
                    *surviving_poll_i = *poll_i;
                }
                surviving_client_i++;
                surviving_poll_i++;
            }
        }
        if (surviving_client_i != client_i)
        {
            PGOTO(clients_resize(&clients, (size_t)(surviving_client_i - clients.p)));
            PGOTO(polls_resize(&polls, (size_t)(surviving_poll_i - polls.p - 1)));
        }
    }

    failure:
    for (client_i = clients.p; client_i < clients.p + clients.size; client_i++) char_buffer_finalize(&client_i->parser.request.data);
    for (poll_i = polls.p; poll_i < polls.p + polls.size; poll_i++) { if (poll_i->fd != -1) close(poll_i->fd); }
    clients_finalize(&clients);
    polls_finalize(&polls);
    char_buffer_finalize(&input_buffer);
    char_buffer_finalize(&intermediate_buffer.content);
    char_buffer_finalize(&output_buffer);
    return error;
}

