#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/cryptography.h"
#include "../../include/memory.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

static void server_send_low_resources(struct Client *client, int fd, bool max_clients, bool max_memory, bool max_utilization)
{
    struct Error *error;
    struct ConstValue response = ZERO_INIT;
    enum FixedResponse fixed = FR_UNKNOWN;
    bool connection_saturated = false;
    if (max_clients) fixed = FR_MAX_CLIENTS;
    if (max_memory) fixed = FR_MAX_MEMORY;
    if (max_utilization) fixed = FR_MAX_UTILIZATION;
    processor_fixed(fixed, &response);
    PGOTO(server_send_value(&response, fd, &connection_saturated));
    AGOTO(!connection_saturated);
    return;

    failure:
    error_print(error, client);
    error_finalize(error);
}

static struct Error *server_accept_connection(struct ClientBuffer *clients, struct PollBuffer *polls, time_t now,
    unsigned char index, bool max_clients, bool max_memory, bool max_utilization) NODISCARD;
static struct Error *server_accept_connection(struct ClientBuffer *clients, struct PollBuffer *polls, time_t now,
    unsigned char index, bool max_clients, bool max_memory, bool max_utilization)
{
    struct ExError exerror;
    const size_t old_size = clients->size;
    struct Client new_client = ZERO_INIT;
    struct pollfd new_poll = { -1, 0, 0 };
    socklen_t socket_address_size;
    int flags;

    /* Create poll */
    socket_address_size = sizeof(new_client.address);
    new_poll.fd = accept(polls->p[index].fd, (struct sockaddr*)&new_client.address, &socket_address_size);
    EXAGOTO0(new_poll.fd >= 0, "accept() failed", EEF_DIE);
    flags = fcntl(new_poll.fd, F_GETFL, 0);
    EXAGOTO0(flags >= 0, "fcntl() failed", EEF_DIE);
    EXAGOTO0(fcntl(new_poll.fd, F_SETFL, flags | O_NONBLOCK) >= 0, "fcntl() failed", EEF_DIE);
    if (max_clients || max_memory || max_utilization)
    {
        if (!ACCEPTING_SOCKET_IS_HTTPS(index)) server_send_low_resources(&new_client, new_poll.fd, max_clients, max_memory, max_utilization);
        close(new_poll.fd);
        return OK;
    }

    /* Create client */
    new_client.last_request_complete = now;
    new_client.last_request_stream_not_empty = now;
    new_client.last_response_stream_not_full = now;
    new_client.last_response_complete = now;
    if (ACCEPTING_SOCKET_IS_HTTPS(index))
    {
        EXPGOTO(cryptography_initialize(&new_client));
    }

    EXPGOTOF(polls_append(polls, &new_poll, 1), EEF_DIE);
    EXPGOTOF(clients_append(clients, &new_client, 1), EEF_DIE);
    return OK;

    failure:
    poll_finalize(&new_poll);
    client_finalize(&new_client);
    polls->size = old_size + ACCEPTING_SOCKETS;
    clients->size = old_size;
    if ((exerror.flags & EEF_LOG) != 0) /* TODO: what are we doing? Head hurts, refactor later */
    {
        error_print(exerror.error, &new_client);
    }
    if (exerror.flags & EEF_DIE)
    {
        return exerror.error;
    }
    error_finalize(exerror.error);
    return OK;
}

struct Error *server_accept_connections(struct ClientBuffer *clients, struct PollBuffer *polls, double utilization, time_t now)
{
    const bool max_clients = clients->size >= MAX_CLIENTS;
    const bool max_memory = g_memory >= MAX_MEMORY_USAGE;
    const bool max_utilization = utilization >= MAX_PROCESSOR_UTILIZATION;
    unsigned char index;
    
    for (index = 0; index < ACCEPTING_SOCKETS; index++)
    {
        ARET0((polls->p[index].revents & (POLLERR | POLLHUP)) == 0, "listening socket failed");
        if ((polls->p[index].revents & POLLIN) == 0) continue;
        PRET(server_accept_connection(clients, polls, now, index, max_clients, max_memory, max_utilization));
    }
    return OK;
}
