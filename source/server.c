#include "../include/server.h"
#include "../include/client.h"
#include "../include/constants.h"
#include "../include/memory.h"
#include "../include/request_parser.h"
#include "../include/request_processor.h"
#include "../include/ring.h"
#include "../include/tables.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Values for error_flags */
enum ErrorFlags
{
    ERF_MESSAGE = 1 << ERROR_TYPE_BITS, /* Message the client before closing connection */
    ERF_LOG     = 2 << ERROR_TYPE_BITS, /* Log the incident */
    ERF_CLOSE   = 4 << ERROR_TYPE_BITS, /* Close the connection */
    ERF_FATAL   = 8 << ERROR_TYPE_BITS, /* Total failure */

    ERF_DEFAULT = ERF_LOG | ERF_CLOSE
};

/* First N sockets are reserved, polls is N entries longer than clients */
#define RESERVED_SOCKETS 1

/* Check whether timeout is still in the future and update remaining time if so */
static bool check_timeout(time_t timeout, time_t now, int *remaining)
{
    if (timeout >= now)
    {
        return false;
    }
    else
    {
        const int local_remaining = (int)(timeout - now);
        if (local_remaining < *remaining) *remaining = local_remaining;
        return true;
    }
}

/* Sends the given value */
static struct Error *send_value(struct ConstValue *value, int fd, bool *sent_not_all) NODISCARD;
static struct Error *send_value(struct ConstValue *value, int fd, bool *sent_not_all)
{
    /* sent_not_all is a bit redundant because one may just check value size, but I'll keep the flag style to be similar with receive_value() */
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        ssize_t signed_sent; size_t sent;
        if (value->parts[i].size == 0) continue;
        signed_sent = send(fd, value->parts[i].p, value->parts[i].size, 0);
        ARET0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed"); /* Failure -> close/log (decided on higher level) */
        sent = (signed_sent >= 0) ? (size_t)signed_sent : 0;
        value->parts[i].p += sent;
        value->parts[i].size -= sent;
        if (value->parts[i].size > 0) { *sent_not_all = true; break; }
    }
    return OK;
}

/* Receives the given amount of bytes to ring */
static struct Error *receive_value(int *error_flags, struct Ring *ring, int fd, size_t size, bool *received_not_all, bool *received_zero) NODISCARD;
static struct Error *receive_value(int *error_flags, struct Ring *ring, int fd, size_t size, bool *received_not_all, bool *received_zero)
{
    struct Value value;
    unsigned char i;

    /* Get buffer */
    PRET(ring_reserve(ring, ring->size + size)); /* Failure -> close */
    *error_flags = ERF_LOG | ERF_CLOSE | ERF_FATAL;
    PRET(ring_push_get(ring, size, &value)); /* Failure -> fatal */
    *error_flags = ERF_DEFAULT;

    /* Receive */
    for (i = 0; i < VALUE_PARTS; i++)
    {
        ssize_t signed_received; size_t received;
        if (value.parts[i].size == 0) continue;
        signed_received = read(fd, value.parts[i].p, value.parts[i].size);
        ARET0(signed_received >= 0, "read() failed"); /* Failure -> close */
        received = (size_t)signed_received;
        value.parts[i].p += received;
        value.parts[i].size -= received;
        if (value.parts[i].size > 0) { *received_not_all = true; *received_zero = (received == 0); break; }
    }

    /* Check for unused buffer */
    *error_flags = ERF_LOG | ERF_CLOSE | ERF_FATAL;
    PRET(ring_unpush(ring, value_size(&value))); /* Failure -> fatal */
    *error_flags = ERF_DEFAULT;
    return OK;
}

/* Sends 'low on resources' response */
static void send_low_resources(struct Client *client, int fd, bool max_clients, bool max_memory, bool max_utilization)
{
    struct Error *error;
    struct ConstValue response = ZERO_INIT;
    enum ErrorType reason;
    bool sent_not_all = false;
    if (max_clients) reason = ERR_MAX_CLIENTS;
    else if (max_memory) reason = ERR_MAX_MEMORY;
    else if (max_utilization) reason = ERR_MAX_UTILIZATION;
    else reason = ERR_UNKNOWN;
    request_processor_error(reason, &response.parts[0]);
    PGOTO(send_value(&response, fd, &sent_not_all)); /* Failure -> log (locally) */
    AGOTO(!sent_not_all); /* Failure -> log (locally) */
    return;

    failure:
    error_print(error, client);
    error_finalize(error);
}

/* Creates listening sockets and puts them into polls */
static struct Error *create_listening_sockets(struct PollBuffer *polls) NODISCARD;
static struct Error *create_listening_sockets(struct PollBuffer *polls)
{
    struct Error *error;
    struct pollfd new_poll = { -1, POLLIN, 0 };
    struct sockaddr_in socket_address = ZERO_INIT;
    int one = 1;

    new_poll.fd = socket(AF_INET, SOCK_STREAM, 0);
    AGOTO0(new_poll.fd >= 0, "socket() failed"); /* Failure -> fatal */
    AGOTO0(setsockopt(new_poll.fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) >= 0, "setsockopt() failed"); /* Failure -> fatal */

    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(8080);
    AGOTO0(bind(new_poll.fd, (struct sockaddr*)&socket_address, sizeof(socket_address)) >= 0, "bind() failed"); /* Failure -> fatal */
    AGOTO0(listen(new_poll.fd, MAX_CLIENTS_IN_QUEUE) >= 0, "listed() failed"); /* Failure -> fatal */
    PGOTO(polls_append(polls, &new_poll, 1)); /* Failure -> fatal */
    return OK;

    failure:
    poll_finalize(&new_poll);
    return error;
}

/* Accepts new connections and puts them into clients */
static struct Error *process_listening_sockets(struct ClientBuffer *clients, struct PollBuffer *polls, double utilization, time_t now) NODISCARD;
static struct Error *process_listening_sockets(struct ClientBuffer *clients, struct PollBuffer *polls, double utilization, time_t now)
{
    struct Error *error;
    bool max_clients, max_memory, max_utilization;
    struct pollfd new_poll = { -1, 0, 0 };
    struct Client new_client = ZERO_INIT;
    socklen_t socket_address_size = sizeof(new_client.address);
    
    AGOTO0((polls->p[0].revents & (POLLERR | POLLHUP)) == 0, "listening socket failed"); /* Failure -> fatal */
    if ((polls->p[0].revents & POLLIN) == 0) return OK;

    /* Create poll */
    new_poll.fd = accept(polls->p[0].fd, (struct sockaddr*)&new_client.address, &socket_address_size);
    AGOTO0(new_poll.fd >= 0, "accept() failed"); /* Failure -> fatal */
    AGOTO0(fcntl(new_poll.fd, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed"); /* Failure -> fatal */
    max_clients = clients->size >= MAX_CLIENTS;
    max_memory = g_memory >= MAX_MEMORY_USAGE;
    max_utilization = utilization >= MAX_PROCESSOR_UTILIZATION;
    if (max_clients || max_memory || max_utilization)
    {
        send_low_resources(&new_client, new_poll.fd, max_clients, max_memory, max_utilization);
        close(new_poll.fd);
        return OK;
    }
    PGOTO(clients_append(clients, &new_client, 1)); /* Failure -> fatal */

    /* Create client */
    new_client.last_input_complete = now;
    new_client.last_input_not_empty = now;
    new_client.last_output_not_full = now;
    new_client.last_output_complete = now;
    PGOTO(polls_append(polls, &new_poll, 1)); /* Failure -> fatal */
    return OK;

    failure:
    poll_finalize(&new_poll);
    clients->size = polls->size - RESERVED_SOCKETS; /* No finalizer because nothing is allocated */
    return error;
}

/* Allocates client's input buffer and reads MIN_AVAILABLE_INPUT to MAX_INPUT_BUFFER_SIZE bytes of data into it */
static struct Error *client_receive_data(int *error_flags, struct Client *client, int fd, time_t now, bool *received_not_all, bool *received_zero) NODISCARD;
static struct Error *client_receive_data(int *error_flags, struct Client *client, int fd, time_t now, bool *received_not_all, bool *received_zero)
{
    int signed_available; size_t available;

    /* Get how much data is available and allocate buffer */
    ARET0(ioctl(fd, FIONREAD, &signed_available) >= 0, "ioctl() failed"); /* Failure -> close */
    ARET0(signed_available >= 0, "ioctl() failed"); /* Failure -> close */
    available = (size_t)signed_available;
    *error_flags = ERF_MESSAGE | ERR_TOO_FAST | ERF_LOG | ERF_CLOSE;
    ARET0(available <= MAX_AVAILABLE_INPUT, "ioctl() failed"); /* Failure -> send ERR_TOO_FAST and close */
    *error_flags = ERF_DEFAULT;
    if (available < MIN_AVAILABLE_INPUT) available = MIN_AVAILABLE_INPUT;
    if (available > MAX_INPUT_BUFFER_SIZE - client->input_buffer.size) available = MAX_INPUT_BUFFER_SIZE - client->input_buffer.size;

    /* Read data into buffer */
    PRET(receive_value(error_flags, &client->input_buffer, fd, available, received_not_all, received_zero));
    if (client->input_buffer.size > 0) client->last_input_not_empty = now;
    return OK;
}

/* Flushes short output buffer into long output buffer */
static struct Error *client_push_to_long_output_buffer(int *error_flags, struct Client *client, const struct ConstValuePart *data) NODISCARD;
static struct Error *client_push_to_long_output_buffer(int *error_flags, struct Client *client, const struct ConstValuePart *data)
{
    PRET(ring_reserve(&client->output_buffer, client->output_buffer.size + data->size)); /* Failure -> close */
    PRET(ring_reserve(&client->output_size_buffer, client->output_size_buffer.size + sizeof(data->size))); /* Failure -> close */
    *error_flags = ERF_LOG | ERF_CLOSE | ERF_FATAL;
    PRET(ring_push_write(&client->output_buffer, data->size, data->p)); /* Failure -> fatal */
    PRET(ring_push_write(&client->output_size_buffer, sizeof(data->size), (const char*)&data->size)); /* Failure -> fatal */
    *error_flags = ERF_DEFAULT;
    return OK;
}

/* Parses the data in the input buffer, and if the request is complete, processes it and pushes into output buffer */
static struct Error *client_process_data(int *error_flags, struct Client *client, time_t now) NODISCARD;
static struct Error *client_process_data(int *error_flags, struct Client *client, time_t now)
{
    struct ConstValuePart response;

    /* Parse request, quit if the request is incomplete */
    *error_flags = ERF_MESSAGE | ERR_PARSING_FAILED | ERF_LOG | ERF_CLOSE;
    PRET(request_parser_parse(&client->parser, &client->input_buffer)); /* Failure -> send ERR_TOO_LARGE/ERR_PARSING_FAILED and close */
    *error_flags = ERF_DEFAULT;
    if (client->parser.state != RPS_END) return OK;
    client->last_input_complete = now;

    /* Flush short output buffer to long output buffer because we are about to get another portion of data to send */
    if (client->short_output_buffer.size > 0)
    {
        const struct ConstValuePart zero = ZERO_INIT;
        PRET(client_push_to_long_output_buffer(error_flags, client, &client->short_output_buffer));
        client->short_output_buffer = zero;
        request_processor_free();
    }

    /* Generate response */
    *error_flags = ERF_MESSAGE | ERR_UNKNOWN | ERF_LOG | ERF_CLOSE;
    PRET(request_processor_process(&client->input_buffer, &client->parser.request, &response)); /* Failure -> send ERR_UNKNOWN and close */
    *error_flags = ERF_LOG | ERF_CLOSE | ERF_FATAL;
    PRET(ring_pop(&client->input_buffer, client->parser.offset)); /* Failure -> fatal */
    *error_flags = ERF_DEFAULT;

    /* Save the response to either short-term or long-term buffer */
    if (client->output_buffer.size > 0)
    {
        PRET(client_push_to_long_output_buffer(error_flags, client, &client->short_output_buffer));
        request_processor_free();
    }
    else
    {
        client->short_output_buffer = response;
    }

    /* Reset parser for future data */
    if (client->parser.request.keep_alive) { const struct RequestParser zero = ZERO_INIT; client->parser = zero; }
    return OK;
}

/* Sends short-term output buffer */
static struct Error *client_send_short_output_buffer(struct Client *client, int fd, time_t now, bool *sent_not_all) NODISCARD;
static struct Error *client_send_short_output_buffer(struct Client *client, int fd, time_t now, bool *sent_not_all)
{
    struct ConstValue value = ZERO_INIT;
    value.parts[0] = client->short_output_buffer;
    PRET(send_value(&value, fd, sent_not_all)); /* Failure -> close */
    client->short_output_buffer = value.parts[0];
    if (value.parts[0].size == 0)
    {
        client->last_output_complete = now;
        request_processor_free();
    }
    return OK;
}

/* Retrieves long-term output buffer */
static struct Error *client_send_long_output_buffer(int *error_flags, struct Client *client, int fd, time_t now, bool *sent_not_all) NODISCARD;
static struct Error *client_send_long_output_buffer(int *error_flags, struct Client *client, int fd, time_t now, bool *sent_not_all)
{
    struct ConstValue value; struct Value nonconst_value;
    size_t old_value_size, new_value_size, sent_size;
    
    /* Send */
    ring_get_all(&client->output_buffer, &nonconst_value);
    value_to_const_value(&value, &nonconst_value);
    old_value_size = value_const_size(&value);
    PRET(send_value(&value, fd, sent_not_all)); /* Failure -> close */
    new_value_size = value_const_size(&value);
    sent_size = old_value_size - new_value_size;
    *error_flags = ERF_LOG | ERF_CLOSE | ERF_FATAL;
    PRET(ring_pop(&client->output_buffer, sent_size)); /* Failure -> fatal */
    *error_flags = ERF_DEFAULT;

    /* Detect sending of a response */
    while (sent_size > 0)
    {
        size_t response_size;
        const struct ValueLocation location = { 0, sizeof(response_size) };
        *error_flags = ERF_LOG | ERF_CLOSE | ERF_FATAL;
        PRET(ring_get(&client->output_size_buffer, &location, false, &nonconst_value)); /* Failure -> fatal */
        *error_flags = ERF_DEFAULT;
        value_read(&nonconst_value, (char*)&response_size);
        if (sent_size >= response_size)
        {
            *error_flags = ERF_LOG | ERF_CLOSE | ERF_FATAL;
            PRET(ring_pop(&client->output_size_buffer, sizeof(response_size))); /* Failure -> fatal */
            *error_flags = ERF_DEFAULT;
            client->last_output_complete = now;
            sent_size -= response_size;
        }
        else
        {
            response_size -= sent_size;
            value_write(&nonconst_value, (const char*)&response_size);
            sent_size = 0;
        }
    }
    return OK;
}

/* Sends the client's output buffer */
static struct Error *client_send_data(int *error_flags, struct Client *client, int fd, time_t now, bool *sent_not_all) NODISCARD;
static struct Error *client_send_data(int *error_flags, struct Client *client, int fd, time_t now, bool *sent_not_all)
{
    if (client->short_output_buffer.size > 0)
    {
        PRET(client_send_short_output_buffer(client, fd, now, sent_not_all));
    }
    if (!*sent_not_all && client->output_buffer.size > 0)
    {
        PRET(client_send_long_output_buffer(error_flags, client, fd, now, sent_not_all));
    }
    if (client->short_output_buffer.size + client->output_buffer.size < MAX_OUTPUT_BUFFER_SIZE)
    {
        client->last_output_not_full = now;
    }
    return OK;
}

static size_t client_process_output_buffer_size(const struct Client *client)
{
    return client->short_output_buffer.size + client->output_buffer.size;
}

static bool client_process_makes_sense_to_receive(struct Client *client)
{
    return client->parser.state != RPS_END
        && client_process_output_buffer_size(client) < MAX_OUTPUT_BUFFER_SIZE;
}

static bool client_process_makes_sense_to_parse(struct Client *client)
{
    return client->parser.state != RPS_END
        && client->input_buffer.size > client->parser.offset
        && client_process_output_buffer_size(client) < MAX_OUTPUT_BUFFER_SIZE;
}

static bool client_process_makes_sense_to_send(struct Client *client)
{
    return client_process_output_buffer_size(client) > 0;
}

/* Processes client, call when revents has something to it */
static struct Error *client_process(struct Client *client, struct pollfd *poll, int *remaining, time_t now) NODISCARD;
static struct Error *client_process(struct Client *client, struct pollfd *poll, int *remaining, time_t now)
{
    struct Error *error;
    int error_flags = ERF_DEFAULT;
    bool receive_may_succeed, send_may_succeed;

    /* Check if connection failed */
    AGOTO0(((poll->revents & (POLLERR | POLLHUP)) == 0), "Connection failed"); /* Failure -> close */

    /* Check if connection should be terminated */
    error_flags = ERF_MESSAGE | ERR_INCOMPLETE_INPUT | ERF_LOG | ERF_CLOSE;
    AGOTO(check_timeout(client->last_input_complete  + MAX_INCOMPLETE_INPUT_TIME,   now, remaining)); /* Failure -> send ERR_INCOMPLETE_INPUT and close */
    error_flags = ERF_DEFAULT;
    AGOTO(check_timeout(client->last_input_not_empty + MAX_NO_INPUT_TIME,           now, remaining)); /* Failure -> close */
    AGOTO(check_timeout(client->last_output_not_full + MAX_FULL_OUTPUT_TIME,        now, remaining)); /* Failure -> close */
    AGOTO(check_timeout(client->last_output_complete + MAX_INCOMPLETE_OUTPUT_TIME,  now, remaining)); /* Failure -> close */
    
    /* Do everything you can */
    receive_may_succeed = !(((poll->events & POLLIN) != 0) && ((poll->revents & POLLIN) == 0));
    send_may_succeed = !(((poll->events & POLLOUT) != 0) && ((poll->revents & POLLOUT) == 0));
    while (true)
    {
        bool done = true;

        /*
        Receive if:
        - read() hasn't failed yet
        - connection is kept alive
        - enough output buffer
        */
        if (receive_may_succeed && client_process_makes_sense_to_receive(client))
        {
            bool received_not_all = false, received_zero = false;
            PGOTO(client_receive_data(&error_flags, client, poll->fd, now, &received_not_all, &received_zero));
            if (received_not_all) receive_may_succeed = false;
            if (received_zero)
            {
                /* Termination for Connection: keep-alive */
                error_flags = ERF_LOG | ERF_CLOSE;
                AGOTO(client->input_buffer.size == 0);
                AGOTO(client->parser.state == RPS_END);
                AGOTO(client_process_output_buffer_size(client) == 0);
                client_finalize(client);
                poll_finalize(poll);
                return OK;
            }
            done = false;
        }
        
        /*
        Parse and process if:
        - connection is kept alive
        - some new data arrived
        - enough output buffer
        */
        if (client_process_makes_sense_to_parse(client))
        {
            PGOTO(client_process_data(&error_flags, client, now));
            done = false;
        }

        /*
        Send if:
        - read() hasn't failed yet
        - non-empty output buffer
        */
        if (send_may_succeed && client_process_makes_sense_to_send(client))
        {
            bool sent_not_all = false;
            PGOTO(client_send_data(&error_flags, client, poll->fd, now, &sent_not_all));
            if (sent_not_all) send_may_succeed = false;
            if (client->parser.state == RPS_END && client_process_output_buffer_size(client) == 0)
            {
                /* Termination for Connection: close */
                error_flags = ERF_LOG | ERF_CLOSE;
                AGOTO(client->input_buffer.size == 0);
                client_finalize(client);
                poll_finalize(poll);
                return OK;
            }
            done = false;
        }

        /* Nothing to do, prepare to wait */
        if (done)
        {
            poll->events = POLLHUP | POLLERR;
            if (client_process_makes_sense_to_receive(client)) poll->events |= POLLIN;
            if (client_process_makes_sense_to_send(client)) poll->events |= POLLOUT;
            return OK;
        }
    }

    failure:
    if ((error_flags & ERF_MESSAGE) != 0)
    {
        struct ConstValuePart response;
        request_processor_error(error_flags & ERROR_TYPE_MASK, &response);
        (void)send(poll->fd, response.p, response.size, 0);
    }
    if ((error_flags & ERF_CLOSE) != 0)
    {
        client_finalize(client);
        poll_finalize(poll);
    }
    if ((error_flags & ERF_LOG) != 0)
    {
        error_print(error, client);
    }
    if ((error_flags & ERF_FATAL) != 0)
    {
        return error;
    }
    error_finalize(error);
    return OK;
}

/* Processes all clients */
static struct Error *clients_process(struct ClientBuffer *clients, struct PollBuffer *polls, clock_t *utilization_clock, double *utilization, int *remaining) NODISCARD;
static struct Error *clients_process(struct ClientBuffer *clients, struct PollBuffer *polls, clock_t *utilization_clock, double *utilization, int *remaining)
{
    clock_t poll_begin_clock, poll_end_clock, process_end_clock;
    time_t now;
    double new_utilization;
    int event_number;
    struct Client *client_i;

    /* Wait for events */
    poll_begin_clock = *utilization_clock; /* Do not repeat clock() call */
    event_number = poll(polls->p, polls->size, (*remaining == INT_MAX) ? -1 : *remaining * 1000);
    ARET0(event_number >= 0, "poll() failed"); /* Failure -> fatal */
    poll_end_clock = clock();
    now = time(NULL);

    /* Accept new connections */
    PRET(process_listening_sockets(clients, polls, *utilization, now));

    /* Process events */
    *remaining = INT_MAX;
    clients_shuffle(clients);
    for (client_i = clients->p; client_i < clients->p + clients->size; client_i++)
    {
        const size_t shuffle_index = client_i->shuffle_index;
        PRET(client_process(clients->p + shuffle_index, polls->p + shuffle_index + 1, remaining, now));
    }
    clients_remove_finalized(clients, polls);

    /* Count utilization */
    process_end_clock = clock();
    new_utilization = (double)(poll_end_clock - poll_begin_clock) / (double)(process_end_clock - poll_begin_clock);
    *utilization = PROCESSOR_UTILIZATION_UPD * new_utilization + (1.0 - PROCESSOR_UTILIZATION_UPD) * *utilization;
    *utilization_clock = process_end_clock;
    return OK;
}

struct Error *server_main(void)
{
    struct Error *error;
    struct ClientBuffer clients = ZERO_INIT;
    struct PollBuffer polls = ZERO_INIT;
    double utilization = 0.0;
    clock_t utilization_clock;
    int remaining = INT_MAX;

    /* Initialize */
    tables_module_initialize();

    /* Create socket */
    PGOTO(create_listening_sockets(&polls));
    
    /* Loop */
    utilization_clock = clock();
    while (true)
    {
        PGOTO(clients_process(&clients, &polls, &utilization_clock, &utilization, &remaining));
    }
    error = OK;

    failure:
    clients_finalize(&clients);
    polls_finalize(&polls);
    request_processor_finalize();

    return error;
}

