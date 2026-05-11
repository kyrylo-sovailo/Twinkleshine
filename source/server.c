#include "../include/server.h"
#include "../include/client.h"
#include "../include/constants.h"
#include "../include/extended_error.h"
#include "../include/memory.h"
#include "../include/output.h"
#include "../include/parser.h"
#include "../include/random.h"
#include "../include/ring.h"
#include "../include/tables.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Sends the given value (Failure -> close and log) */
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
        ARET0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed");
        sent = (signed_sent >= 0) ? (size_t)signed_sent : 0;
        value->parts[i].p += sent;
        value->parts[i].size -= sent;
        if (value->parts[i].size > 0) { *sent_not_all = true; break; }
    }
    return OK;
}

/* Receives the given amount of bytes to ring */
static struct ExError receive_value(struct Ring *ring, int fd, size_t size, bool *received_not_all, bool *received_zero) NODISCARD;
static struct ExError receive_value(struct Ring *ring, int fd, size_t size, bool *received_not_all, bool *received_zero)
{
    const struct ExError EXOK = { OK };
    struct Value value;
    unsigned char i;

    /* Get buffer */
    EXPRETF(ring_reserve(ring, ring->size + size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_get(ring, size, &value), EEF_CLOSE_LOG_DIE);

    /* Receive */
    for (i = 0; i < VALUE_PARTS; i++)
    {
        ssize_t signed_received; size_t received;
        if (value.parts[i].size == 0) continue;
        signed_received = read(fd, value.parts[i].p, value.parts[i].size);
        EXARET0(signed_received >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "read() failed", EEF_CLOSE_LOG);
        received = (signed_received >= 0) ? (size_t)signed_received : 0;
        value.parts[i].p += received;
        value.parts[i].size -= received;
        if (value.parts[i].size > 0) { *received_not_all = true; *received_zero = (signed_received == 0); break; }
    }

    /* Check for unused buffer */
    EXPRETF(ring_unpush(ring, value_size(&value)), EEF_CLOSE_LOG_DIE);
    return EXOK;
}

/* Sends 'low on resources' response (Failure -> log) */
static void send_low_resources(struct Client *client, int fd, bool max_clients, bool max_memory, bool max_utilization)
{
    struct Error *error;
    struct ConstValue response = ZERO_INIT;
    enum FixedResponse fixed = FR_UNKNOWN;
    bool sent_not_all = false;
    if (max_clients) fixed = FR_MAX_CLIENTS;
    if (max_memory) fixed = FR_MAX_MEMORY;
    if (max_utilization) fixed = FR_MAX_UTILIZATION;
    processor_fixed(fixed, &response);
    PGOTO(send_value(&response, fd, &sent_not_all));
    AGOTO(!sent_not_all);
    close(fd);
    return;

    failure:
    error_print(error, client);
    error_finalize(error);
    close(fd);
}

/* Creates one listening socket (Failure -> die) */
static struct Error *create_listening_socket(struct PollBuffer *polls, const struct sockaddr *address, const struct sockaddr *reserve, socklen_t address_sizeof)
{
    struct Error *error;
    const int one = 1;
    struct pollfd new_poll = { -1, POLLIN, 0 };

    new_poll.fd = socket(address->sa_family, SOCK_STREAM, 0);
    AGOTO0(new_poll.fd >= 0, "socket() failed");
    AGOTO0(setsockopt(new_poll.fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) >= 0, "setsockopt() failed");
    if (address->sa_family == AF_INET6)
    {
        AGOTO0(setsockopt(new_poll.fd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one)) >= 0, "setsockopt() failed");
    }
    if (!(bind(new_poll.fd, address, address_sizeof) >= 0))
    {
        AGOTO0(bind(new_poll.fd, reserve, address_sizeof) >= 0, "bind() failed");
    }
    AGOTO0(listen(new_poll.fd, MAX_CLIENTS_IN_QUEUE) >= 0, "listed() failed");
    PGOTO(polls_append(polls, &new_poll, 1));
    return OK;

    failure:
    poll_finalize(&new_poll);
    return error;
}

/* Creates listening sockets (Failure -> die) */
static struct Error *create_listening_sockets(struct PollBuffer *polls) NODISCARD;
static struct Error *create_listening_sockets(struct PollBuffer *polls)
{
    struct sockaddr_in ipv4 = ZERO_INIT, reserved_ipv4 = ZERO_INIT;
    struct sockaddr_in6 ipv6 = ZERO_INIT, reserved_ipv6 = ZERO_INIT;

    ARET0(signal(SIGPIPE, SIG_IGN) != SIG_ERR, "signal() failed");

    ipv4.sin_family = AF_INET;
    ipv4.sin_addr.s_addr = INADDR_ANY;
    ipv4.sin_port = htons(80);
    reserved_ipv4 = ipv4;
    reserved_ipv4.sin_port = htons(8080);
    PRET(create_listening_socket(polls, (struct sockaddr*)&ipv4, (struct sockaddr*)&reserved_ipv4, sizeof(ipv4)));
    
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_addr = in6addr_any;
    ipv6.sin6_port = ipv4.sin_port;
    reserved_ipv6 = ipv6;
    reserved_ipv6.sin6_port = reserved_ipv4.sin_port;
    PRET(create_listening_socket(polls, (struct sockaddr*)&ipv6, (struct sockaddr*)&reserved_ipv6, sizeof(ipv6)));
    
    return OK;
}

/* Accepts new connections and puts them into clients (Failure -> die) */
static struct Error *process_listening_sockets(struct ClientBuffer *clients, struct PollBuffer *polls, double utilization, time_t now) NODISCARD;
static struct Error *process_listening_sockets(struct ClientBuffer *clients, struct PollBuffer *polls, double utilization, time_t now)
{
    struct Error *error;
    const bool max_clients = clients->size >= MAX_CLIENTS;
    const bool max_memory = g_memory >= MAX_MEMORY_USAGE;
    const bool max_utilization = utilization >= MAX_PROCESSOR_UTILIZATION;
    struct Client new_client = ZERO_INIT;
    struct pollfd new_poll = { -1, 0, 0 };
    unsigned char i;

    new_client.last_request_complete = now;
    new_client.last_request_stream_not_empty = now;
    new_client.last_response_stream_not_full = now;
    new_client.last_request_complete = now;
    
    for (i = 0; i < ACCEPTING_SOCKETS; i++)
    {
        socklen_t socket_address_size = sizeof(new_client.address);
        AGOTO0((polls->p[i].revents & (POLLERR | POLLHUP)) == 0, "listening socket failed");
        if ((polls->p[i].revents & POLLIN) == 0) continue;

        /* Create poll */
        new_poll.fd = accept(polls->p[i].fd, (struct sockaddr*)&new_client.address, &socket_address_size);
        AGOTO0(new_poll.fd >= 0, "accept() failed");
        AGOTO0(fcntl(new_poll.fd, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed");
        if (max_clients || max_memory || max_utilization)
        {
            send_low_resources(&new_client, new_poll.fd, max_clients, max_memory, max_utilization);
            continue;
        }
        PGOTO(polls_append(polls, &new_poll, 1));

        /* Create client */
        PGOTO(clients_append(clients, &new_client, 1));
    }
    return OK;

    failure:
    poll_finalize(&new_poll);
    clients->size = polls->size - ACCEPTING_SOCKETS; /* No finalizer because nothing is allocated */
    return error;
}

/* Allocates client's input buffer and reads MIN_AVAILABLE_REQUEST_STREAM to MAX_REQUEST_STREAM_SIZE bytes of data into it */
static struct ExError client_receive_data(struct Client *client, int fd, time_t now, bool *received_not_all, bool *received_zero) NODISCARD;
static struct ExError client_receive_data(struct Client *client, int fd, time_t now, bool *received_not_all, bool *received_zero)
{
    const struct ExError EXOK = { OK };
    int signed_available; size_t available;

    /* Get how much data is available and allocate buffer */
    EXARET0(ioctl(fd, FIONREAD, &signed_available) >= 0, "ioctl() failed", EEF_CLOSE_LOG);
    EXARET0(signed_available >= 0, "ioctl() failed", EEF_CLOSE_LOG);
    available = (size_t)signed_available;
    EXARET0(available <= MAX_AVAILABLE_REQUEST_STREAM, "ioctl() failed", EEF_SEND_CLOSE_LOG(FR_MAX_AVAILABLE_REQUEST_STREAM));
    if (available < MIN_AVAILABLE_REQUEST_STREAM) available = MIN_AVAILABLE_REQUEST_STREAM;
    if (available > MAX_REQUEST_STREAM_SIZE - client->request_stream.size) available = MAX_REQUEST_STREAM_SIZE - client->request_stream.size;

    /* Read data into buffer */
    EXPRET(receive_value(&client->request_stream, fd, available, received_not_all, received_zero));
    if (client->request_stream.size > 0) client->last_request_stream_not_empty = now;
    return EXOK;
}

/* Flushes short output buffer into long output buffer */
static struct ExError client_push_to_long_response_stream(struct Client *client, const struct ConstValue *data) NODISCARD;
static struct ExError client_push_to_long_response_stream(struct Client *client, const struct ConstValue *data)
{
    const struct ExError EXOK = { OK };
    unsigned char i;
    EXPRETF(ring_reserve(&client->response_stream, client->response_stream.size + value_const_size(data)), EEF_CLOSE_LOG);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(&client->response_stream, data->parts[i].size, data->parts[i].p), EEF_CLOSE_LOG_DIE);
    }
    return EXOK;
}

/* Parses the data in the input buffer, and if the request is complete, processes it and pushes into output buffer */
static struct ExError client_process_data(struct Client *client, time_t now) NODISCARD;
static struct ExError client_process_data(struct Client *client, time_t now)
{
    const struct ExError EXOK = { OK };
    struct ConstValue response_stream;

    /* Parse request, quit if the request is incomplete */
    EXPRET(parser_parse(&client->parser, &client->request, &client->request_stream));
    if (client->parser.state != RPS_END) return EXOK;
    client->last_request_complete = now;

    /* Flush short output buffer to long output buffer because we are about to get another portion of data to send */
    if (g_short_response_stream_owner != NULL)
    {
        const struct ConstValue zero = ZERO_INIT;
        EXPRET(client_push_to_long_response_stream(g_short_response_stream_owner, &g_short_response_stream));
        g_short_response_stream = zero;
        g_short_response_stream_owner = NULL;
        processor_free();
    }

    /* Generate response */
    EXPRET(processor_process(&client->request, &client->request_stream, (client->response_queue.size == 0) ? &client->response : NULL, &client->response_queue, &response_stream));
    EXPRETF(ring_pop(&client->request_stream, client->request.stream_size), EEF_CLOSE_LOG_DIE);

    /* Save the response to either short-term or long-term buffer */
    if (client->response_stream.size == 0)
    {
        g_short_response_stream = response_stream;
        g_short_response_stream_owner = client;
    }
    else
    {
        EXPRET(client_push_to_long_response_stream(client, &response_stream));
        processor_free();
    }

    /* Reset parser for future data */
    if (client->response.keep_alive)
    {
        const struct Request zero = ZERO_INIT;
        const struct Parser zero2 = ZERO_INIT;
        client->request = zero;
        client->parser = zero2;
    }
    return EXOK;
}

/* Prints and pops response metadata from response_queue (Failure -> close, log, and die) */
static struct Error *client_pop_response(struct Client *client)
{
    struct Value method, resource;
    PRET(ring_get(&client->response_queue, &client->response.method, false, &method));
    PRET(ring_get(&client->response_queue, &client->response.resource, false, &resource));
    output_open(false);
    output_print(false, "Success %.*s%.*s %.*s%.*s\n",
        (int)method.parts[0].size, method.parts[0].p, (int)method.parts[1].size, method.parts[1].p,
        (int)resource.parts[0].size, resource.parts[0].p, (int)resource.parts[1].size, resource.parts[1].p);
    output_print_client(false, client);
    output_print_time(true);
    output_print(true, COMMON_N);
    output_close(false);
    PRET(ring_pop(&client->response_queue, client->response.size));
    ARET(client->response_queue.size == 0);
    return OK;
}

/* Sends short-term output buffer (Failure -> close and log) */
static struct ExError client_send_short_response_stream(struct Client *client, int fd, time_t now, bool *sent_not_all) NODISCARD;
static struct ExError client_send_short_response_stream(struct Client *client, int fd, time_t now, bool *sent_not_all)
{
    const struct ExError EXOK = { OK };

    /* Send chunk of data */
    EXPRETF(send_value(&g_short_response_stream, fd, sent_not_all), EEF_CLOSE_LOG);

    /* See if it was the chunk was last */
    if (value_const_size(&g_short_response_stream) == 0)
    {
        g_short_response_stream_owner = NULL;
        processor_free();
        EXPRETF(client_pop_response(client), EEF_CLOSE_LOG_DIE);
        client->last_response_complete = now;
    }
    return EXOK;
}

/* Retrieves long-term output buffer */
static struct ExError client_send_long_output_buffer(struct Client *client, int fd, time_t now, bool *sent_not_all) NODISCARD;
static struct ExError client_send_long_output_buffer(struct Client *client, int fd, time_t now, bool *sent_not_all)
{
    const struct ExError EXOK = { OK };
    struct ConstValue value; struct Value nonconst_value;
    size_t old_value_size, new_value_size, sent_stream_size;
    
    /* Send chunk of data */
    ring_get_all(&client->response_stream, &nonconst_value);
    value_to_const_value(&value, &nonconst_value);
    old_value_size = value_const_size(&value);
    EXPRETF(send_value(&value, fd, sent_not_all), EEF_CLOSE_LOG);
    new_value_size = value_const_size(&value);
    sent_stream_size = old_value_size - new_value_size;
    EXPRETF(ring_pop(&client->response_stream, sent_stream_size), EEF_CLOSE_LOG_DIE);

    /* Analyze what was in that chunk of data */
    while (sent_stream_size > 0)
    {
        if (sent_stream_size >= client->response.stream_size)
        {
            /* Sent */
            EXPRETF(client_pop_response(client), EEF_CLOSE_LOG_DIE);
            client->last_response_complete = now;
            sent_stream_size -= client->response.stream_size;
            EXARET(client->response_queue.size == 0 || client->response_queue.size >= sizeof(struct Response), EEF_CLOSE_LOG_DIE);
            if (client->response_queue.size > 0)
            {
                EXPRETF(ring_pop_read(&client->response_queue, sizeof(struct Response), (char*)&client->response), EEF_CLOSE_LOG_DIE);
            }
        }
        else
        {
            /* Not sent */
            client->response.stream_size -= sent_stream_size;
            sent_stream_size = 0;
        }
    }
    return EXOK;
}

/* Sends the client's output buffer */
static struct ExError client_send_data(struct Client *client, int fd, time_t now, bool *sent_not_all) NODISCARD;
static struct ExError client_send_data(struct Client *client, int fd, time_t now, bool *sent_not_all)
{
    const struct ExError EXOK = { OK };
    size_t short_response_stream_size = 0;
    if (client == g_short_response_stream_owner)
    {
        EXPRET(client_send_short_response_stream(client, fd, now, sent_not_all));
        short_response_stream_size = value_const_size(&g_short_response_stream);
    }
    if (!*sent_not_all && client->response_stream.size > 0)
    {
        EXPRET(client_send_long_output_buffer(client, fd, now, sent_not_all));
    }
    if (short_response_stream_size + client->response_stream.size < MAX_RESPONSE_STREAM_SIZE)
    {
        client->last_response_stream_not_full = now;
    }
    return EXOK;
}

static size_t client_process_response_stream_size(const struct Client *client)
{
    size_t size = client->response_stream.size;
    if (client == g_short_response_stream_owner) size += value_const_size(&g_short_response_stream);
    return size;
}

static bool client_process_makes_sense_to_receive(struct Client *client)
{
    return client->parser.state != RPS_END
        && client_process_response_stream_size(client) < MAX_RESPONSE_STREAM_SIZE;
}

static bool client_process_makes_sense_to_parse(struct Client *client)
{
    return client->parser.state != RPS_END
        && client->request_stream.size > client->request.stream_size
        && client_process_response_stream_size(client) < MAX_RESPONSE_STREAM_SIZE;
}

static bool client_process_makes_sense_to_send(struct Client *client)
{
    return client_process_response_stream_size(client) > 0;
}

static struct Error *client_process_check_one(time_t timeout, time_t now, int *remaining) NODISCARD;
static struct Error *client_process_check_one(time_t timeout, time_t now, int *remaining)
{
    if (remaining == NULL)
    {
        /* Check */
        ARET(timeout >= now);
    }
    else
    {
        /* Arm */
        const int local_remaining = (int)(timeout - now);
        if (local_remaining < *remaining) *remaining = local_remaining;
    }
    return OK;
}

static struct ExError client_process_check(struct Client *client, time_t now, int *remaining) NODISCARD;
static struct ExError client_process_check(struct Client *client, time_t now, int *remaining)
{
    const struct ExError EXOK = { OK };
    if (client->response_stream.size > 0)
    {
        EXPRETF(client_process_check_one(client->last_request_complete + MAX_INCOMPLETE_REQUEST_TIME, now, remaining),
            EEF_SEND_CLOSE_LOG(FR_MAX_INCOMPLETE_REQUEST_TIME));
    }
    if (client->request_stream.size == 0)
    {
        EXPRETF(client_process_check_one(client->last_request_stream_not_empty + MAX_EMPTY_REQUEST_STREAM_TIME, now, remaining),
            EEF_CLOSE_LOG);
    }
    if (client_process_response_stream_size(client) >= MAX_RESPONSE_STREAM_SIZE)
    {
        EXPRETF(client_process_check_one(client->last_response_stream_not_full + MAX_FULL_RESPONSE_STREAM_TIME, now, remaining),
            EEF_CLOSE_LOG);
    }
    if (client_process_response_stream_size(client) > 0)
    {
        EXPRETF(client_process_check_one(client->last_response_complete + MAX_INCOMPLETE_RESPONSE_TIME, now, remaining),
            EEF_CLOSE_LOG);
    }
    return EXOK;
}

/* Processes client, call when revents has something to it (Failure -> die) */
static struct Error *client_process(struct Client *client, struct pollfd *poll, int *remaining, time_t now) NODISCARD;
static struct Error *client_process(struct Client *client, struct pollfd *poll, int *remaining, time_t now)
{
    struct ExError exerror;
    bool receive_may_succeed, send_may_succeed;

    /* Check if connection failed */
    EXAGOTO0(((poll->revents & (POLLERR | POLLHUP)) == 0), "Connection failed", EEF_CLOSE_LOG);

    /* Check if connection should be terminated */
    EXPGOTO(client_process_check(client, now, NULL));
    
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
            EXPGOTO(client_receive_data(client, poll->fd, now, &received_not_all, &received_zero));
            if (received_not_all) receive_may_succeed = false;
            if (received_zero)
            {
                /* Termination for Connection: keep-alive */
                EXAGOTO(client->request_stream.size == 0, EEF_CLOSE_LOG);
                EXAGOTO(client->parser.state == (enum ParserState)0, EEF_CLOSE_LOG);
                EXAGOTO(client_process_response_stream_size(client) == 0, EEF_CLOSE_LOG);
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
            EXPGOTO(client_process_data(client, now));
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
            EXPGOTO(client_send_data(client, poll->fd, now, &sent_not_all));
            if (sent_not_all) send_may_succeed = false;
            if (client->parser.state == RPS_END && client_process_response_stream_size(client) == 0)
            {
                /* Termination for Connection: close */
                EXAGOTO(client->request_stream.size == 0, EEF_CLOSE_LOG);
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
            EXPIGNORE(client_process_check(client, now, remaining));
            return OK;
        }
    }

    failure:
    if ((exerror.flags & EEF_SEND) != 0)
    {
        struct ConstValue response;
        unsigned char i;
        processor_fixed((enum FixedResponse)exerror.flags & (enum FixedResponse)~0xFF, &response);
        for (i = 0; i < VALUE_PARTS; i++)
        {
            if (response.parts[i].size > 0) (void)send(poll->fd, response.parts[i].p, response.parts[i].size, 0);
        }
    }
    if ((exerror.flags & EEF_CLOSE) != 0)
    {
        client_finalize(client);
        poll_finalize(poll);
    }
    if ((exerror.flags & EEF_LOG) != 0)
    {
        error_print(exerror.error, client);
    }
    if ((exerror.flags & EEF_DIE) != 0)
    {
        return exerror.error;
    }
    error_finalize(exerror.error);
    return OK;
}

/* Processes all clients (Failure -> die) */
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
    ARET0(event_number >= 0, "poll() failed");
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
        PRET(client_process(clients->p + shuffle_index, polls->p + shuffle_index + ACCEPTING_SOCKETS, remaining, now));
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
    random_module_initialize();
    tables_module_initialize();
    PGOTO(processor_module_initialize());
    PGOTO(create_listening_sockets(&polls));
    output_open(false);
    output_print(false, "Twinkleshine started\n");
    output_print_time(false);
    output_print(false, "\n");
    output_close(false);
    
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
    processor_module_finalize();

    return error;
}

