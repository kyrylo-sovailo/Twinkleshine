#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/cryptography.h"
#include "../../include/random.h"
#include "../../include/tables.h"

#include <netinet/in.h>

#include <limits.h>

static bool get_makes_sense_to_receive(const struct Client *client)
{
    return client->cryptography_state != CS_SHUTDOWN
        && (client->parser.state != RPS_END || ACCEPTING_SOCKET_IS_MESSAGE(client->accepting_socket)) /* TODO: ????? */
        && client_response_stream_size(client) < MAX_RESPONSE_STREAM_SIZE;
}

static bool get_makes_sense_to_parse(const struct Client *client)
{
    return client->cryptography_state == CS_OPERATIONAL
        && client->parser.state != RPS_END
        && client->request_stream.size > client->request.stream_size
        && client_response_stream_size(client) < MAX_RESPONSE_STREAM_SIZE;
}

static bool get_makes_sense_to_send(const struct Client *client)
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
            EEF_SEND_SHUTDOWN_LOG(FR_MAX_INCOMPLETE_REQUEST_TIME));
    }
    if (client->request_stream.size == 0)
    {
        EXPRETF(check_timeout(client->last_request_stream_not_empty + MAX_EMPTY_REQUEST_STREAM_TIME, now, remaining),
            EEF_SHUTDOWN_LOG);
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

static struct ExError server_process_client_round_recovery(struct Client *client, enum FixedResponse fixed) NODISCARD;
static struct ExError server_process_client_round_recovery(struct Client *client, enum FixedResponse fixed)
{
    const struct ExError EXOK = { OK };
    struct Response response;
    struct Value response_stream;
    EXPRET(processor_fixed(client->accepting_socket, fixed, &response, &client->response_queue, &response_stream));
    if (client->response_count == 0) client->response = response;
    EXPRET(cryptography_encrypt(client, &response, &response_stream));
    client->response_count++;
    EXPRET(cryptography_finalize(client));
    return EXOK;
}

static struct ExError server_process_client_round(struct Client *client, struct pollfd *poll, time_t now, enum ConnectionFlag *flags, bool *first, bool *done_something) NODISCARD;
static struct ExError server_process_client_round(struct Client *client, struct pollfd *poll, time_t now, enum ConnectionFlag *flags, bool *first, bool *done_something)
{
    const struct ExError EXOK = { OK };
    struct ExError exerror;

    if (*first)
    {
        *first = false;

        /* Check if connection failed */
        EXAGOTO0(((poll->revents & (POLLERR | POLLHUP)) == 0), "Connection failed", EEF_CLOSE_LOG);

        /* Check if connection should be terminated */
        EXPGOTO(check_timeouts(client, now, NULL));
    }

    /*
    Receive if:
    - read() hasn't failed yet
    - connection is kept alive
    - enough output buffer
    */
    if ((*flags & CF_EXHAUSTED) == 0 && (*flags & CF_CLOSED) == 0 && get_makes_sense_to_receive(client))
    {
        *done_something = true;
        EXPGOTO(server_receive_data(client, poll->fd, now, flags));
        if ((*flags & CF_CLOSED) != 0
        || client->cryptography_state == CS_SHUTDOWN
        || (client->cryptography_state == CS_PENDING_SHUTDOWN && client_response_stream_size(client) == 0))
        {
            /* Client terminated connection or initiated TLS shutdown or completed TLS shutdown */
            /* TODO: In HTTP, client shouldn't terminate "Connection: close" connections */
            EXAGOTO(client->request_stream.size == 0, EEF_CLOSE_LOG);
            EXAGOTO(client->parser.state == 0 || client->parser.state == RPS_END, EEF_CLOSE_LOG);
            EXAGOTO(client_response_stream_size(client) == 0, EEF_CLOSE_LOG);
            EXAGOTO(client->response_queue.size == 0, EEF_CLOSE_LOG);
            EXAGOTO(client->response_count == 0, EEF_CLOSE_LOG);
            if (ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket)) poll_finalize(poll);
            client_finalize(client);
            return EXOK;
        }
    }
    
    /*
    Parse and process if:
    - connection is kept alive
    - some new data arrived
    - enough output buffer
    */
    if (get_makes_sense_to_parse(client))
    {
        *done_something = true;
        EXPGOTO(server_process_data(client, now));
        if (client->ssl != NULL && client->parser.state == RPS_END)
        {
            /* Server should shutdown connection */
            EXPGOTO(cryptography_finalize(client));
        }
    }

    /*
    Send if:
    - read() hasn't failed yet
    - non-empty output buffer
    */
    if (((*flags & CF_SATURATED) == 0) && get_makes_sense_to_send(client))
    {
        *done_something = true;
        EXPGOTO(server_send_data(client, poll->fd, now, flags));
        if ((client->ssl == NULL && client->parser.state == RPS_END && client_response_stream_size(client) == 0)
        || (client->ssl != NULL && client->cryptography_state == CS_SHUTDOWN && client_response_stream_size(client) == 0)) /* TODO: am I sure? */
        {
            /* Server should close connection */
            EXAGOTO(client->request_stream.size == 0, EEF_CLOSE_LOG);
            EXAGOTO(client->response_queue.size == 0, EEF_CLOSE_LOG);
            EXAGOTO(client->response_count == 0, EEF_CLOSE_LOG);
            if (ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket)) poll_finalize(poll);
            client_finalize(client);
            return EXOK;
        }
    }

    return EXOK;

    failure:
    if (client->ssl != NULL && client->cryptography_state != CS_PENDING_SHUTDOWN && (exerror.flags & PARTIAL_EEF_SHUTDOWN) != 0)
    {
        /* Recoverable error, perform shutdown instead of close */
        *done_something = true;
        error_print(exerror.error, client);
        error_finalize(exerror.error);
        EXPRET(server_process_client_round_recovery(client, (enum FixedResponse)exerror.flags & (enum FixedResponse)~0xFF)); /* sic, not EXPGOTO */
        return EXOK;
    }
    
    return exerror;
}

/* Processes client, call when revents has something to it (Failure -> die) */
static struct Error *server_process_client(struct Client *client, struct pollfd *poll, int *remaining, time_t now) NODISCARD;
static struct Error *server_process_client(struct Client *client, struct pollfd *poll, int *remaining, time_t now)
{
    struct ExError exerror;
    enum ConnectionFlag flags = CF_NO;
    bool first = true;

    if ((poll->events & POLLIN) != 0 && (poll->revents & POLLIN) == 0) flags |= CF_EXHAUSTED;
    if (ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket) && (poll->events & POLLOUT) != 0 && (poll->revents & POLLOUT) == 0) flags |= CF_SATURATED;
    while (true)
    {
        bool done_something = false;

        /* Do everything I can */
        EXPGOTO(server_process_client_round(client, poll, now, &flags, &first, &done_something));
        
        /* Nothing to do, prepare to wait */
        if (done_something) continue;
        if (ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket))
        {
            poll->events = POLLHUP | POLLERR;
            if (get_makes_sense_to_receive(client)) poll->events |= POLLIN;
            if (get_makes_sense_to_send(client)) poll->events |= POLLOUT;
        }
        else
        {
            if (get_makes_sense_to_send(client) && *remaining > CHUNK_SEND_REPEAT_TIME) *remaining = CHUNK_SEND_REPEAT_TIME;
        }
        EXPIGNORE(check_timeouts(client, now, remaining));
        return OK;

        failure:
        if ((exerror.flags & PARTIAL_EEF_SEND) != 0 && client->ssl == NULL)
        {
            struct Value response;
            processor_fixed_failsafe(client->accepting_socket, (enum FixedResponse)exerror.flags & (enum FixedResponse)~0xFF, &response);
            if (ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket))
            {
                unsigned char i;
                for (i = 0; i < VALUE_PARTS; i++)
                {
                    if (response.parts[i].size == 0) continue;
                    (void)send(poll->fd, response.parts[i].p, response.parts[i].size, 0);
                }
            }
            else
            {
                (void)sendto(poll->fd, response.parts[0].p, response.parts[0].size, 0,
                    (struct sockaddr*)&client->address, (client->address.ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
            }
        }
        if ((exerror.flags & PARTIAL_EEF_SHUTDOWN) != 0)
        {
            /* Not intercepted? Too bad */
            if (ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket)) poll_finalize(poll);
            client_finalize(client);
        }
        if ((exerror.flags & PARTIAL_EEF_CLOSE) != 0)
        {
            if (ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket)) poll_finalize(poll);
            client_finalize(client);
        }
        if ((exerror.flags & PARTIAL_EEF_LOG) != 0)
        {
            error_print(exerror.error, client);
        }
        if ((exerror.flags & PARTIAL_EEF_DIE) != 0)
        {
            return exerror.error;
        }
        error_finalize(exerror.error);
        return OK;
    }

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
    PRET(server_accept_traffic(clients, polls, *utilization, now));

    /* Process events */
    *remaining = INT_MAX;
    clients_shuffle(clients);
    for (client_i = clients->p; client_i < clients->p + clients->size; client_i++)
    {
        const size_t shuffle_index = client_i->shuffle_index;
        struct Client *client = clients->p + shuffle_index;
        struct pollfd *poll = ACCEPTING_SOCKET_IS_CONNECTION(client->accepting_socket) ? (polls->p + shuffle_index + ACCEPTING_SOCKETS) : (polls->p + client->accepting_socket);
        PRET(server_process_client(client, poll, remaining, now));
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
