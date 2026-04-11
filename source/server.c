#include "../include/server.h"
#include "../include/client.h"
#include "../include/constants.h"
#include "../include/memory.h"
#include "../include/request_parser.h"
#include "../include/request_processor.h"
#include "../include/ring.h"
#include "../commonlib/include/path.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Values for error_flags */
enum ErrorFlags
{
    ERF_FATAL   = 1 << ERROR_TYPE_BITS, /* Total failure */
    ERF_CLOSE   = 2 << ERROR_TYPE_BITS, /* Close the connection */
    ERF_MESSAGE = 4 << ERROR_TYPE_BITS, /* Message the client before closing connection */
    ERF_LOG     = 8 << ERROR_TYPE_BITS  /* Log the incident */
};
#define ERROR_FLAGS_DEFAULT (ERF_CLOSE | ERF_LOG)

/* First N sockets are reserved, polls is N entries longer than clients */
#define RESERVED_SOCKETS 1

/* Sends the given value */
static struct Error *send_value(struct ConstValue *value, int fd, bool *end) NODISCARD;
static struct Error *send_value(struct ConstValue *value, int fd, bool *end)
{
    unsigned char i;
    for (i = 0; i < 2; i++)
    {
        ssize_t signed_sent; size_t sent;
        if (value->parts[i].size == 0) continue;
        signed_sent = send(fd, value->parts[i].p, value->parts[i].size, 0);
        ARET0(signed_sent >= 0 || errno == EAGAIN || errno == EWOULDBLOCK, "send() failed"); /* Failure -> close */
        sent = (signed_sent >= 0) ? (size_t)signed_sent : 0;
        value->parts[i].p += sent;
        value->parts[i].size -= sent;
        if (value->parts[i].size > 0) { *end = true; break; }
    }
    return OK;
}

/* Sends 'low on resources' response */
static void send_low_resources(int fd, bool max_clients, bool max_memory, bool max_utilization)
{
    struct Error *error;
    struct ConstValue response = ZERO_INIT;
    enum ErrorType reason;
    bool sent_not_all = false;
    (void)max_clients;
    if (max_memory) reason = ERR_MAX_MEMORY;
    else if (max_utilization) reason = ERR_MAX_UTILIZATION;
    else reason = ERR_MAX_CLIENTS;
    request_processor_error(reason, &response.parts[0]);
    PGOTO(send_value(&response, fd, &sent_not_all)); /* Failure -> log (locally) */
    AGOTO(!sent_not_all); /* Failure -> log (locally) */
    return;

    failure:
    error_print(error);
    error_finalize(error);
}

/* Creates listening sockets and puts them into polls */
static struct Error *create_listening_sockets(struct PollBuffer *polls) NODISCARD;
static struct Error *create_listening_sockets(struct PollBuffer *polls)
{
    struct Error *error;
    struct pollfd new_poll = { -1, POLLIN, 0 };
    struct sockaddr_in socket_address = ZERO_INIT;

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
        send_low_resources(new_poll.fd, max_clients, max_memory, max_utilization);
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
    if (new_poll.fd >= 0) close(new_poll.fd);
    clients->size = polls->size - RESERVED_SOCKETS;
    return error;
}

/* Allocates client's input buffer and reads data into it */
static struct Error *client_receive_data(int *error_flags, struct Client *client, int fd, bool *received_not_all, bool *received_zero) NODISCARD;
static struct Error *client_receive_data(int *error_flags, struct Client *client, int fd, bool *received_not_all, bool *received_zero)
{
    int signed_available; size_t available;
    struct ValueLocation location; struct Value value;
    unsigned char i;

    /* Get how much data is available and allocate buffer */
    ARET0(ioctl(fd, FIONREAD, &signed_available) >= 0, "ioctl() failed"); /* Failure -> close */
    ARET0(signed_available >= 0, "ioctl() failed"); /* Failure -> close */
    available = (size_t)signed_available;
    *error_flags |=  (ERF_MESSAGE | ERR_TOO_FAST);
    ARET0(available <= MAX_AVAILABLE_INPUT, "ioctl() failed"); /* Failure -> send ERR_TOO_FAST and close */
    *error_flags &= ~(ERF_MESSAGE | ERR_TOO_FAST);
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
        if (value.parts[i].size > 0) { *received_not_all = true; *received_zero = (received == 0); break; }
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
    struct ConstValuePart response;

    /* Parse request */
    *error_flags |=  (ERF_MESSAGE | ERR_PARSING_FAILED);
    PRET(request_parser_parse(&client->parser, &client->input_buffer)); /* Failure -> send ERR_TOO_LARGE/ERR_PARSING_FAILED and close */
    *error_flags &= ~(ERF_MESSAGE | ERR_PARSING_FAILED);
    if (client->parser.state != RPS_END) return OK;

    /* Flush short output buffer to long output buffer */
    if (client->short_output_buffer.size > 0)
    {
        const struct ConstValuePart zero = ZERO_INIT;
        PRET(ring_reserve(&client->output_buffer, client->output_buffer.size + client->short_output_buffer.size)); /* Failure -> close */
        *error_flags |=  ERF_FATAL;
        PRET(ring_push(&client->output_buffer, client->short_output_buffer.size, client->short_output_buffer.p)); /* Failure -> fatal */
        *error_flags &= ~ERF_FATAL;
        client->short_output_buffer = zero;
        request_processor_free();
    }

    /* Generate response */
    *error_flags |=  (ERF_MESSAGE | ERR_UNKNOWN);
    PRET(request_processor_process(&client->input_buffer, &client->parser.request, &response)); /* Failure -> send ERR_UNKNOWN and close */
    *error_flags &= ~(ERF_MESSAGE | ERR_UNKNOWN);
    *error_flags |=  ERF_FATAL;
    PRET(ring_pop(&client->input_buffer, client->parser.offset)); /* Failure -> fatal */
    *error_flags &= ~ERF_FATAL;
    if (!client->parser.request.keep_alive) { const struct RequestParser zero = ZERO_INIT; client->parser = zero; }

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

/* Sends the client's output buffer */
static struct Error *client_send_data(int *error_flags, struct Client *client, int fd, bool *sent_not_all) NODISCARD;
static struct Error *client_send_data(int *error_flags, struct Client *client, int fd, bool *sent_not_all)
{
    if (client->short_output_buffer.size > 0)
    {
        /* Using short-term buffer */
        struct ConstValue value = ZERO_INIT;
        value.parts[0] = client->short_output_buffer;
        PRET(send_value(&value, fd, sent_not_all)); /* Failure -> close */
        client->short_output_buffer = value.parts[0];
        if (value.parts[0].size == 0) request_processor_free();
    }
    else
    {
        /* Using long-term buffer */
        struct ConstValue value;
        struct Value nonconst_value;
        size_t initial_value_size;
        unsigned char i;
        ring_get_all(&client->output_buffer, &nonconst_value);
        for (i = 0; i < 2; i++) { value.parts[i].p = nonconst_value.parts[i].p; value.parts[i].size = nonconst_value.parts[i].size; }
        initial_value_size = value.parts[0].size + value.parts[1].size;
        PRET(send_value(&value, fd, sent_not_all)); /* Failure -> close */
        *error_flags |=  ERF_FATAL;
        PRET(ring_pop(&client->output_buffer, initial_value_size - (value.parts[0].size + value.parts[1].size))); /* Failure -> fatal */
        *error_flags &= ~ERF_FATAL;
    }
    return OK;
}

/* Processes client, call when revents has something to it */
static struct Error *client_process(struct Client *client, struct pollfd *poll, time_t now) NODISCARD;
static struct Error *client_process(struct Client *client, struct pollfd *poll, time_t now)
{
    struct Error *error;
    int error_flags = ERROR_FLAGS_DEFAULT;
    bool receive_may_succeed, send_may_succeed;
    bool makes_sense_to_receive, makes_sense_to_parse, makes_sense_to_send;
    bool done, termination;
    (void)now;

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
        - connection is kept alive
        - enough output buffer
        */
        makes_sense_to_receive = client->parser.state != RPS_END
            && (client->short_output_buffer.size + client->output_buffer.size) < MAX_OUTPUT_BUFFER_SIZE;
        if (receive_may_succeed && makes_sense_to_receive)
        {
            bool received_not_all = false, received_zero = false;
            PGOTO(client_receive_data(&error_flags, client, poll->fd, &received_not_all, &received_zero));
            if (received_not_all) receive_may_succeed = false;
            if (received_zero) { termination = true; break; }
            done = false;
        }
        
        /*
        Parse and process if:
        - connection is kept alive
        - some new data arrived
        - enough output buffer
        */
        makes_sense_to_parse = client->parser.state != RPS_END
            && client->input_buffer.size > client->parser.offset
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
            bool sent_not_all = false;
            PGOTO(client_send_data(&error_flags, client, poll->fd, &sent_not_all));
            if (sent_not_all) send_may_succeed = false;
            if (client->parser.state == RPS_END && (client->short_output_buffer.size + client->output_buffer.size) == 0) { termination = true; break; }
            done = false;
        }

        /* Nothing to do */
        if (done) break;
    }

    if (termination)
    {
        /* Normal termination */
        client_finalize(client);
        poll_finalize(poll);
    }
    else
    {
        /* Arming triggers */
        poll->events = POLLHUP | POLLERR;
        makes_sense_to_receive = client->parser.state != RPS_END
            && (client->short_output_buffer.size + client->output_buffer.size) < MAX_OUTPUT_BUFFER_SIZE;
        makes_sense_to_send = (client->short_output_buffer.size + client->output_buffer.size) > 0;
        if (makes_sense_to_receive) poll->events |= POLLIN;
        if (makes_sense_to_send) poll->events |= POLLOUT;
    }
    return OK;

    failure:
    if ((error_flags & ERF_LOG) != 0)
    {
        error_print(error);
    }
    if ((error_flags & ERF_MESSAGE) != 0)
    {
        struct ConstValuePart response;
        request_processor_error(error_flags & ((1 << ERROR_TYPE_BITS) - 1), &response);
        (void)send(poll->fd, response.p, response.size, 0);
    }
    if ((error_flags & ERF_CLOSE) != 0)
    {
        client_finalize(client);
        poll_finalize(poll);
    }
    if ((error_flags & ERF_FATAL) != 0)
    {
        return error;
    }
    return OK;
}

/* Processes all clients */
static struct Error *clients_process(struct ClientBuffer *clients, struct PollBuffer *polls, clock_t *utilization_clock, double *utilization) NODISCARD;
static struct Error *clients_process(struct ClientBuffer *clients, struct PollBuffer *polls, clock_t *utilization_clock, double *utilization)
{
    clock_t poll_begin_clock, poll_end_clock, process_end_clock;
    time_t now;
    double new_utilization;
    int event_number;
    struct Client *client_i;

    /* Wait for events */
    poll_begin_clock = *utilization_clock; /* Do not repeat clock() call */
    event_number = poll(polls->p, polls->size, -1);
    ARET0(event_number >= 0, "poll() failed"); /* Failure -> fatal */
    poll_end_clock = clock();
    now = time(NULL);

    /* Accept new connections */
    PRET(process_listening_sockets(clients, polls, *utilization, now));

    /* Process events */
    clients_shuffle(clients);
    for (client_i = clients->p; client_i < clients->p + clients->size; client_i++)
    {
        const size_t shuffle_index = client_i->shuffle_index;
        PRET(client_process(clients->p + shuffle_index, polls->p + shuffle_index + 1, now));
    }
    clients_remove_finalized(clients, polls);

    /* Count utilization */
    process_end_clock = clock();
    new_utilization = (double)(poll_end_clock - poll_begin_clock) / (double)(process_end_clock - poll_begin_clock);
    *utilization = PROCESSOR_UTILIZATION_UPD * new_utilization + (1.0 - PROCESSOR_UTILIZATION_UPD) * *utilization;
    *utilization_clock = process_end_clock;
    return OK;
}

struct Error *server_main(int argc, char **argv)
{
    struct Error *error;
    struct ClientBuffer clients = ZERO_INIT;
    struct PollBuffer polls = ZERO_INIT;
    double utilization = 0.0;
    clock_t utilization_clock;

    /* Initialize modules */
    PGOTO(path_module_initialize(argc, argv));

    /* Create socket */
    PGOTO(create_listening_sockets(&polls));
    
    /* Loop */
    utilization_clock = clock();
    while (true)
    {
        PGOTO(clients_process(&clients, &polls, &utilization_clock, &utilization));
    }
    error = OK;

    failure:
    clients_finalize(&clients);
    polls_finalize(&polls);
    request_processor_finalize();
    path_module_finalize();

    return error;
}

