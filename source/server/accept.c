#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/cryptography.h"
#include "../../include/memory.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

static void server_send_low_resources(unsigned char index, struct Client *client, int fd, bool max_clients, bool max_memory, bool max_utilization)
{
    struct Error *error;
    enum FixedResponse fixed = FR_UNKNOWN;
    enum ConnectionFlag flags = CF_NO;
    struct ConstValue response_stream;
    if (ACCEPTING_SOCKET_IS_HTTPS(index) || ACCEPTING_SOCKET_IS_GEMINI(index)) return;
    if (max_clients) fixed = FR_MAX_CLIENTS;
    if (max_memory) fixed = FR_MAX_MEMORY;
    if (max_utilization) fixed = FR_MAX_UTILIZATION;
    if (ACCEPTING_SOCKET_IS_HTTP(index)) processor_fixed_failsafe(CT_HTTP, fixed, &response_stream);
    else if (ACCEPTING_SOCKET_IS_GOPHER(index)) processor_fixed_failsafe(CT_GOPHER, fixed, &response_stream);
    else if (ACCEPTING_SOCKET_IS_FINGER(index)) processor_fixed_failsafe(CT_FINGER, fixed, &response_stream);
    else /* if (ACCEPTING_SOCKET_IS_NEX(index)) */ processor_fixed_failsafe(CT_NEX, fixed, &response_stream);
    PGOTO(server_send_value(&response_stream, fd, &flags));
    AGOTO((flags & CF_SATURATED) == 0);
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
    EXAGOTO0(new_poll.fd >= 0, "accept() failed", EEF_CLOSE_LOG);
    flags = fcntl(new_poll.fd, F_GETFL, 0);
    EXAGOTO0(flags >= 0, "fcntl() failed", EEF_CLOSE_LOG);
    EXAGOTO0(fcntl(new_poll.fd, F_SETFL, flags | O_NONBLOCK) >= 0, "fcntl() failed", EEF_CLOSE_LOG);
    if (max_clients || max_memory || max_utilization)
    {
        server_send_low_resources(index, &new_client, new_poll.fd, max_clients, max_memory, max_utilization);
        close(new_poll.fd);
        return OK;
    }

    /* Create client */
    new_client.last_request_complete = now;
    new_client.last_request_stream_not_empty = now;
    new_client.last_response_stream_not_full = now;
    new_client.last_response_complete = now;
    if (ACCEPTING_SOCKET_IS_HTTP(index))
    {
        new_client.type = CT_HTTP;
        new_client.cryptography_state = CS_OPERATIONAL;
    }
    else if (ACCEPTING_SOCKET_IS_HTTPS(index))
    {
        new_client.type = CT_HTTP;
        EXPGOTO(cryptography_initialize(&new_client));
    }
    else if (ACCEPTING_SOCKET_IS_GOPHER(index))
    {
        new_client.type = CT_GOPHER;
        new_client.cryptography_state = CS_OPERATIONAL;
    }
    else if (ACCEPTING_SOCKET_IS_FINGER(index))
    {
        new_client.type = CT_FINGER;
        new_client.cryptography_state = CS_OPERATIONAL;
    }
    else if (ACCEPTING_SOCKET_IS_GEMINI(index))
    {
        new_client.type = CT_GEMINI;
        EXPGOTO(cryptography_initialize(&new_client));
    }
    else /* if (ACCEPTING_SOCKET_IS_NEX(index)) */
    {
        new_client.type = CT_NEX;
        new_client.cryptography_state = CS_OPERATIONAL;
    }

    EXPGOTOF(polls_append(polls, &new_poll, 1), EEF_CLOSE_LOG);
    EXPGOTOF(clients_append(clients, &new_client, 1), EEF_CLOSE_LOG);
    return OK;

    failure:
    poll_finalize(&new_poll);
    client_finalize(&new_client);
    polls->size = old_size + ACCEPTING_SOCKETS;
    clients->size = old_size;
    if ((exerror.flags & PARTIAL_EEF_SEND)      != 0) { /* Do nothing, never happens  */ }
    if ((exerror.flags & PARTIAL_EEF_SHUTDOWN)  != 0) { /* Do nothing, already closed */ }
    if ((exerror.flags & PARTIAL_EEF_CLOSE)     != 0) { /* Do nothing, already closed */ }
    if ((exerror.flags & PARTIAL_EEF_LOG)       != 0) error_print(exerror.error, &new_client);
    if ((exerror.flags & PARTIAL_EEF_DIE)       != 0) return exerror.error;
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
