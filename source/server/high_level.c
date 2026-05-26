#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/cryptography.h"
#include "../../include/random.h"
#include "../../include/tables.h"

#include <limits.h>

static bool get_makes_sense_to_receive(struct Client *client)
{
    return client->parser.state != RPS_END
        && client_response_stream_size(client) < MAX_RESPONSE_STREAM_SIZE;
}

static bool get_makes_sense_to_parse(struct Client *client)
{
    return client->parser.state != RPS_END
        && client->request_stream.size > client->request.stream_size
        && client_response_stream_size(client) < MAX_RESPONSE_STREAM_SIZE;
}

static bool get_makes_sense_to_send(struct Client *client)
{
    return client_response_stream_size(client) > 0;
}

/* Checks or calculates all timeouts (Failure -> [send], close and log) */
static struct Error *check_timeout(time_t timeout, time_t now, int *remaining) NODISCARD;
static struct Error *check_timeout(time_t timeout, time_t now, int *remaining)
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

/* Checks or calculates all timeouts */
static struct ExError check_timeouts(struct Client *client, time_t now, int *remaining) NODISCARD;
static struct ExError check_timeouts(struct Client *client, time_t now, int *remaining)
{
    const struct ExError EXOK = { OK };
    if (client->response_stream.size > 0)
    {
        EXPRETF(check_timeout(client->last_request_complete + MAX_INCOMPLETE_REQUEST_TIME, now, remaining),
            EEF_SEND_CLOSE_LOG(FR_MAX_INCOMPLETE_REQUEST_TIME));
    }
    if (client->request_stream.size == 0)
    {
        EXPRETF(check_timeout(client->last_request_stream_not_empty + MAX_EMPTY_REQUEST_STREAM_TIME, now, remaining),
            EEF_CLOSE_LOG);
    }
    if (client_response_stream_size(client) >= MAX_RESPONSE_STREAM_SIZE)
    {
        EXPRETF(check_timeout(client->last_response_stream_not_full + MAX_FULL_RESPONSE_STREAM_TIME, now, remaining),
            EEF_CLOSE_LOG);
    }
    if (client_response_stream_size(client) > 0)
    {
        EXPRETF(check_timeout(client->last_response_complete + MAX_INCOMPLETE_RESPONSE_TIME, now, remaining),
            EEF_CLOSE_LOG);
    }
    return EXOK;
}

/* Processes client, call when revents has something to it (Failure -> die) */
static struct Error *server_process_client(struct Client *client, struct pollfd *poll, int *remaining, time_t now) NODISCARD;
static struct Error *server_process_client(struct Client *client, struct pollfd *poll, int *remaining, time_t now)
{
    struct ExError exerror;
    bool receive_may_succeed, send_may_succeed;

    /* Check if connection failed */
    EXAGOTO0(((poll->revents & (POLLERR | POLLHUP)) == 0), "Connection failed", EEF_CLOSE_LOG);

    /* Check if connection should be terminated */
    EXPGOTO(check_timeouts(client, now, NULL));
    
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
        if (receive_may_succeed && get_makes_sense_to_receive(client))
        {
            bool connection_exhausted = false, connection_closed = false;
            EXPGOTO(server_receive_data(client, poll->fd, now, &connection_exhausted, &connection_closed));
            if (connection_exhausted) receive_may_succeed = false;
            if (connection_closed)
            {
                /* Termination for Connection: keep-alive */
                EXAGOTO(client->request_stream.size == 0, EEF_CLOSE_LOG);
                EXAGOTO(client->parser.state == (enum ParserState)0, EEF_CLOSE_LOG);
                EXAGOTO(client_response_stream_size(client) == 0, EEF_CLOSE_LOG);
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
        if (get_makes_sense_to_parse(client))
        {
            EXPGOTO(server_process_data(client, now));
            done = false;
        }

        /*
        Send if:
        - read() hasn't failed yet
        - non-empty output buffer
        */
        if (send_may_succeed && get_makes_sense_to_send(client))
        {
            bool connection_saturated = false;
            EXPGOTO(server_send_data(client, poll->fd, now, &connection_saturated));
            if (connection_saturated) send_may_succeed = false;
            if (client->parser.state == RPS_END && client_response_stream_size(client) == 0)
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
            if (get_makes_sense_to_receive(client)) poll->events |= POLLIN;
            if (get_makes_sense_to_send(client)) poll->events |= POLLOUT;
            EXPIGNORE(check_timeouts(client, now, remaining));
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
    if ((exerror.flags & EEF_SHUTDOWN) != 0)
    {
        /* TODO: error handling is atrocious */
        client_finalize(client);
        poll_finalize(poll);
    }
    if ((exerror.flags & EEF_CLOSE) != 0)
    {
        client_finalize(client);
        poll_finalize(poll);
    }
    if ((exerror.flags & EEF_LOG) != 0)
    {
        /* TODO: same problem as shutdown: EEF_LOG is not fatal */
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
static struct Error *server_process_clients(struct ClientBuffer *clients, struct PollBuffer *polls, clock_t *utilization_clock, double *utilization, int *remaining) NODISCARD;
static struct Error *server_process_clients(struct ClientBuffer *clients, struct PollBuffer *polls, clock_t *utilization_clock, double *utilization, int *remaining)
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
    PRET(server_accept_connections(clients, polls, *utilization, now));

    /* Process events */
    *remaining = INT_MAX;
    clients_shuffle(clients);
    for (client_i = clients->p; client_i < clients->p + clients->size; client_i++)
    {
        const size_t shuffle_index = client_i->shuffle_index;
        PRET(server_process_client(clients->p + shuffle_index, polls->p + shuffle_index + ACCEPTING_SOCKETS, remaining, now));
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
    PGOTO(cryptography_module_initialize());
    PGOTO(processor_module_initialize());
    PGOTO(server_initialize(&polls));
    
    /* Loop */
    utilization_clock = clock();
    while (true)
    {
        PGOTO(server_process_clients(&clients, &polls, &utilization_clock, &utilization, &remaining));
    }
    error = OK;

    failure:
    server_finalize(&clients, &polls);
    processor_module_finalize();
    cryptography_module_finalize();

    return error;
}
