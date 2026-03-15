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

/* Values for error_flags */
enum ErrorFlags
{
    ERF_FATAL   = 1 << ERROR_TYPE_BITS, /* Total failure */
    ERF_CLOSE   = 2 << ERROR_TYPE_BITS, /* Close the connection */
    ERF_MESSAGE = 4 << ERROR_TYPE_BITS, /* Message the client before closing connection */
    ERF_LOG     = 8 << ERROR_TYPE_BITS  /* Log the incident */
};
#define ERROR_FLAGS_DEFAULT (ERF_CLOSE | ERF_LOG)

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
    socket_address.sin_port = htons(8080);
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

/* Allocates client's input buffer and reads data into it, does nothing with  */
static struct Error *client_receive_data(int *error_flags, struct Client *client, int fd, bool *end, bool *total_end) NODISCARD;
static struct Error *client_receive_data(int *error_flags, struct Client *client, int fd, bool *end, bool *total_end)
{
    int signed_available; size_t available;
    struct ValueLocation location; struct Value value;
    unsigned char i;

    /* Get how much data is available and allocate buffer */
    ARET0(ioctl(fd, FIONREAD, &signed_available) >= 0, "ioctl() failed"); /* Failure -> close */
    ARET0(signed_available >= 0, "ioctl() failed"); /* Failure -> close */
    available = (size_t)signed_available;
    *error_flags |=  (ERR_TOO_FAST | ERF_MESSAGE);
    ARET0(available <= MAX_AVAILABLE_INPUT, "ioctl() failed"); /* Failure -> send ERR_TOO_FAST and close */
    *error_flags &= ~(ERR_TOO_FAST | ERF_MESSAGE);
    if (available < MIN_AVAILABLE_INPUT) available = MIN_AVAILABLE_INPUT;
    if (available > MAX_INPUT_BUFFER_SIZE - client->input_buffer.size) available = MAX_INPUT_BUFFER_SIZE - client->input_buffer.size;
    PRET(ring_reserve(&client->input_buffer, client->input_buffer.size + available)); /* Failure -> close */

    /* Read data into buffer */
    location.offset = 0;
    location.size = available;
    *error_flags |=  ERF_FATAL;
    PRET(ring_push(&client->input_buffer, available, NULL)); /* Failure -> fatal */
    PRET(ring_get(&client->input_buffer, &location, true, &value)); /* Failure -> fatal */
    *error_flags &= ~ERF_FATAL;
    for (i = 0; i < 2; i++)
    {
        ssize_t signed_received; size_t received;
        if (value.parts[i].size == 0) continue;
        signed_received = read(fd, value.parts[i].p, value.parts[i].size);
        ARET0(signed_received >= 0, "read() failed"); /* Failure -> close */
        received = (size_t)signed_received;
        value.parts[i].p += received;
        value.parts[i].size -= received;
        if (value.parts[i].size > 0) { *total_end |= (received == 0); *end = true; break; }
    }
    *error_flags |=  ERF_FATAL;
    PRET(ring_unpush(&client->input_buffer, value.parts[0].size + value.parts[1].size)); /* Failure -> fatal */
    *error_flags &= ~ERF_FATAL;
    return OK;
}

/* Parse the data in the input buffer, try to sent it, and put in the output buffer if failed */
static struct Error *client_process_data(int *error_flags, struct Client *client) NODISCARD;
static struct Error *client_process_data(int *error_flags, struct Client *client)
{
    struct ValuePart response;

    /* Parse request */
    *error_flags |=  (ERR_PARSING_FAILED | ERF_MESSAGE);
    PRET(request_parser_parse(&client->parser, &client->input_buffer)); /* Failure -> send ERR_TOO_LARGE/ERR_PARSING_FAILED and close */
    *error_flags &= ~(ERR_PARSING_FAILED | ERF_MESSAGE);
    if (client->parser.state != RPS_END) return OK;

    /* Flush short output buffer to long output buffer */
    if (client->short_output_buffer.size > 0)
    {
        PRET(ring_reserve(&client->output_buffer, client->output_buffer.size + client->short_output_buffer.size)); /* Failure -> close */
        *error_flags |=  ERF_FATAL;
        PRET(ring_push(&client->output_buffer, client->short_output_buffer.size, client->short_output_buffer.p)); /* Failure -> fatal */
        *error_flags &= ~ERF_FATAL;
        memset(&client->short_output_buffer, 0, sizeof(struct ValuePart));
        request_processor_free();
    }

    /* Generate response */
    *error_flags |=  (ERR_UNKNOWN | ERF_MESSAGE);
    PRET(request_processor_process(&client->input_buffer, &client->parser.request, &response)); /* Failure -> send ERR_UNKNOWN and close */
    *error_flags &= ~(ERR_UNKNOWN | ERF_MESSAGE);
    *error_flags |=  ERF_FATAL;
    PRET(ring_pop(&client->input_buffer, client->parser.offset)); /* Failure -> fatal */
    *error_flags &= ~ERF_FATAL;
    memset(&client->parser, 0, sizeof(client->parser));

    /* Save the response to either short-term or long-term buffer */
    if (client->output_buffer.size > 0)
    {
        PRET(ring_reserve(&client->output_buffer, client->output_buffer.size + response.size)); /* Failure -> close */
        *error_flags |=  ERF_FATAL;
        PRET(ring_push(&client->output_buffer, response.size, response.p)); /* Failure -> fatal */
        *error_flags &= ~ERF_FATAL;
        request_processor_free();
    }
    else
    {
        client->short_output_buffer = response;
    }
    return OK;
}

/* Sends the output buffer */
static struct Error *client_send_data(int *error_flags, struct Client *client, int fd, bool *end) NODISCARD;
static struct Error *client_send_data(int *error_flags, struct Client *client, int fd, bool *end)
{
    struct Value value;
    size_t initial_value_size;
    unsigned char i;

    /* Get memory that we are trying to send */
    if (client->short_output_buffer.size > 0)
    {
        value.parts[0] = client->short_output_buffer;
        memset(&value.parts[1], 0, sizeof(struct ValuePart));
    }
    else
    {
        ring_get_all(&client->output_buffer, &value);
    }
    initial_value_size = value.parts[0].size + value.parts[1].size;

    /* Send */
    for (i = 0; i < 2; i++)
    {
        ssize_t signed_sent; size_t sent;
        if (value.parts[i].size == 0) continue;
        signed_sent = send(fd, value.parts[i].p, value.parts[i].size, 0);
        ARET0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed"); /* Failure -> close */
        sent = (signed_sent >= 0) ? (size_t)signed_sent : 0;
        value.parts[i].p += sent;
        value.parts[i].size -= sent;
        if (value.parts[i].size > 0) { *end = true; break; }
    }

    /* Cleanup */
    if (client->short_output_buffer.size > 0)
    {
        if (value.parts[0].size > 0) client->short_output_buffer = value.parts[0];
        else { memset(&client->short_output_buffer, 0, sizeof(struct ValuePart)); request_processor_free(); }
    }
    else
    {
        *error_flags |=  ERF_FATAL;
        PRET(ring_pop(&client->output_buffer, initial_value_size - (value.parts[0].size + value.parts[1].size))); /* Failure -> fatal */
        *error_flags &= ~ERF_FATAL;
    }
    return OK;
}

/* Processes client, call when revents has something to it */
static struct Error *client_process(struct Client *client, struct pollfd *poll) NODISCARD;
static struct Error *client_process(struct Client *client, struct pollfd *poll)
{
    struct Error *error;
    int error_flags = ERROR_FLAGS_DEFAULT;
    bool receive_may_succeed, send_may_succeed;
    bool makes_sense_to_receive, makes_sense_to_parse, makes_sense_to_send;
    bool done, termination;

    /* Check if connection failed */
    AGOTO0(((poll->revents & (POLLERR | POLLHUP)) == 0), "Connection failed"); /* Failure -> close */
    
    /* Do everything you can */
    receive_may_succeed = !(((poll->events & POLLIN) != 0) && ((poll->revents & POLLIN) == 0));
    send_may_succeed = !(((poll->events & POLLOUT) != 0) && ((poll->revents & POLLOUT) == 0));
    termination = false;
    while (true)
    {
        done = true;

        /*
        Receive if:
        - read() hasn't failed yet
        - enough output buffer
        */
        makes_sense_to_receive = (client->short_output_buffer.size + client->output_buffer.size) < MAX_OUTPUT_BUFFER_SIZE;
        if (receive_may_succeed && makes_sense_to_receive)
        {
            bool end = false, total_end = false;
            PGOTO(client_receive_data(&error_flags, client, poll->fd, &end, &total_end));
            if (end) receive_may_succeed = false;
            if (total_end) { termination = true; break; }
            done = false;
        }
        
        /*
        Parse and process if:
        - some new data arrived
        - enough output buffer
        */
        makes_sense_to_parse = client->input_buffer.size > client->parser.offset
            && (client->short_output_buffer.size + client->output_buffer.size) < MAX_OUTPUT_BUFFER_SIZE;
        if (makes_sense_to_parse)
        {
            PGOTO(client_process_data(&error_flags, client));
            done = false;
        }

        /*
        Send if:
        - read() hasn't failed yet
        - non-empty output buffer
        */
        makes_sense_to_send = (client->short_output_buffer.size + client->output_buffer.size) > 0;
        if (send_may_succeed && makes_sense_to_send)
        {
            bool end;
            PGOTO(client_send_data(&error_flags, client, poll->fd, &end));
            if (end) send_may_succeed = false;
            done = false;
        }

        /* Nothing to do */
        if (done) break;
    }

    if (termination)
    {
        /* Normal termination */
        if (client->short_output_buffer.size != 0) request_processor_free();
        ring_finalize(&client->input_buffer);
        ring_finalize(&client->output_buffer);
        close(poll->fd);
        poll->fd = -1;
    }
    else
    {
        /* Arming triggers */
        poll->events = POLLHUP | POLLERR;
        if ((client->short_output_buffer.size + client->output_buffer.size) < MAX_OUTPUT_BUFFER_SIZE) poll->events |= POLLIN;
        if ((client->short_output_buffer.size + client->output_buffer.size) > 0) poll->events |= POLLOUT;
    }
    return OK;

    failure:
    if ((error_flags & ERF_LOG) != 0)
    {
        error_print(error);
    }
    if ((error_flags & ERF_MESSAGE) != 0)
    {
        struct ValuePart response;
        request_processor_error(error_flags & ((1 << ERROR_TYPE_BITS) - 1), &response);
        (void)send(poll->fd, response.p, response.size, 0);
    }
    if ((error_flags & ERF_CLOSE) != 0)
    {
        if (client->short_output_buffer.size != 0) request_processor_free();
        ring_finalize(&client->input_buffer);
        ring_finalize(&client->output_buffer);
        close(poll->fd);
        poll->fd = -1;
    }
    if ((error_flags & ERF_FATAL) != 0)
    {
        return error;
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

    /* Accept new connections */
    PRET(process_listening_sockets(clients, polls));

    /* Shuffle events */
    shuffle_clients(clients->p, clients->size);

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
    clients->size -= (size_t)(surviving_client_i - client_i);
    polls->size -= (size_t)(surviving_poll_i - poll_i);
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

