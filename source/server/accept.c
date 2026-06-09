#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/cryptography.h"
#include "../../include/memory.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>

static void server_send_low_resources_connection(unsigned char index, const struct Client *client, int fd, bool max_clients, bool max_memory, bool max_utilization)
{
    struct Error *error;
    enum FixedResponse fixed = FR_UNKNOWN;
    enum ConnectionFlag flags = CF_NO;
    struct Value response_stream;
    if (ACCEPTING_SOCKET_IS_HTTPS(index) || ACCEPTING_SOCKET_IS_GEMINI(index)) return;
    if (max_clients) fixed = FR_MAX_CLIENTS;
    if (max_memory) fixed = FR_MAX_MEMORY;
    if (max_utilization) fixed = FR_MAX_UTILIZATION;
    if (ACCEPTING_SOCKET_IS_HTTP(index)) processor_fixed_http_failsafe(fixed, &response_stream);
    else if (ACCEPTING_SOCKET_IS_GOPHER(index)) processor_fixed_gopher_failsafe(fixed, &response_stream);
    else if (ACCEPTING_SOCKET_IS_FINGER(index)) processor_fixed_finger_failsafe(fixed, &response_stream);
    else if (ACCEPTING_SOCKET_IS_SPARTAN(index)) processor_fixed_spartan_failsafe(fixed, &response_stream);
    else /* if (ACCEPTING_SOCKET_IS_NEX(index)) */ processor_fixed_nex_failsafe(fixed, &response_stream);
    PGOTO(server_send_value(&response_stream, fd, &flags));
    AGOTO((flags & CF_SATURATED) == 0);
    close(fd);
    return;

    failure:
    close(fd);
    error_print(error, client);
    error_finalize(error);
}

static void server_send_low_resources_message(const struct Client *client, int fd, bool max_clients, bool max_memory, bool max_utilization)
{
    enum FixedResponse fixed = FR_UNKNOWN;
    struct Value response_message;
    if (max_clients) fixed = FR_MAX_CLIENTS;
    if (max_memory) fixed = FR_MAX_MEMORY;
    if (max_utilization) fixed = FR_MAX_UTILIZATION;
    processor_fixed_guppy_failsafe(fixed, &response_message);
    (void)sendto(fd, response_message.parts[0].p, response_message.parts[0].size, 0,
        (struct sockaddr*)&client->address, (client->address.ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
}

static bool address_compare(const struct sockaddr_storage *a, const struct sockaddr_storage *b)
{
    if (a->ss_family != b->ss_family)
    {
        return false;
    }
    else if (a->ss_family == AF_INET6)
    {
        return memcmp(&((const struct sockaddr_in6*)a)->sin6_addr, &((const struct sockaddr_in6*)b)->sin6_addr, sizeof(struct in6_addr)) == 0;
    }
    else /* if (a->ss_family == AF_INET) */
    {
        return memcmp(&((const struct sockaddr_in*)a)->sin_addr, &((const struct sockaddr_in*)b)->sin_addr, sizeof(struct in_addr)) == 0;
    }
}

static struct Error *server_accept_connection(struct ClientBuffer *clients, struct PollBuffer *polls, time_t now,
    unsigned char index, bool max_clients, bool max_memory, bool max_utilization) NODISCARD;
static struct Error *server_accept_connection(struct ClientBuffer *clients, struct PollBuffer *polls, time_t now,
    unsigned char index, bool max_clients, bool max_memory, bool max_utilization)
{
    struct ExError exerror;
    const size_t old_size = clients->size;
    struct pollfd new_poll = { -1, 0, 0 };
    struct Client new_client = ZERO_INIT;
    socklen_t socket_address_size = sizeof(new_client.address);
    int flags;

    /* Create poll */
    new_poll.fd = accept(polls->p[index].fd, (struct sockaddr*)&new_client.address, &socket_address_size);
    EXAGOTO0(new_poll.fd >= 0, "accept() failed", EEF_CLOSE_LOG);
    flags = fcntl(new_poll.fd, F_GETFL, 0);
    EXAGOTO0(flags >= 0, "fcntl() failed", EEF_CLOSE_LOG);
    EXAGOTO0(fcntl(new_poll.fd, F_SETFL, flags | O_NONBLOCK) >= 0, "fcntl() failed", EEF_CLOSE_LOG);
    if (max_clients || max_memory || max_utilization)
    {
        server_send_low_resources_connection(index, &new_client, new_poll.fd, max_clients, max_memory, max_utilization);
        return OK;
    }

    /* Create client */
    new_client.accepting_socket = index;
    new_client.last_request_complete = now;
    new_client.last_request_stream_not_empty = now;
    new_client.last_response_stream_not_full = now;
    new_client.last_response_complete = now;
    if (ACCEPTING_SOCKET_IS_HTTPS(index) || ACCEPTING_SOCKET_IS_GEMINI(index))
    {
        EXPGOTO(cryptography_initialize(&new_client));
    }
    else 
    {
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
    if ((exerror.flags & PARTIAL_EEF_DIE)       != 0) return exerror.error; /* Parent function is responsible for logging */
    if ((exerror.flags & PARTIAL_EEF_LOG)       != 0) error_print(exerror.error, &new_client);
    error_finalize(exerror.error);
    return OK;
}

static struct Error *server_accept_message(struct ClientBuffer *clients, struct PollBuffer *polls, time_t now,
    unsigned char index, bool max_clients, bool max_memory, bool max_utilization) NODISCARD;
static struct Error *server_accept_message(struct ClientBuffer *clients, struct PollBuffer *polls, time_t now,
    unsigned char index, bool max_clients, bool max_memory, bool max_utilization)
{
    const struct pollfd new_poll = { -1, 0, POLLIN };
    struct Client new_client = ZERO_INIT;
    socklen_t socket_address_size = sizeof(new_client.address);
    const ssize_t signed_received = recvfrom(polls->p[index].fd, g_short_request_message, sizeof(g_short_request_message), 0,
        (struct sockaddr*)&new_client.address, &socket_address_size);
    if (signed_received >= 0)
    {
        struct Error *error;
        const size_t old_size = clients->size;
        const char guppy[] = "guppy://";
        const size_t guppy_size = sizeof(guppy) - 1;
        const size_t received = (size_t)signed_received;
        struct Client *p;
        for (p = clients->p; p < clients->p + clients->size; p++)
        {
            if (ACCEPTING_SOCKET_IS_GUPPY(p->accepting_socket) && address_compare(&new_client.address, &p->address))
            {
                /* Not a new client */
                g_short_request_message_owner = p;
                g_short_request_message_size = received;
                polls->p[(p - clients->p) + ACCEPTING_SOCKETS].revents |= POLLIN;
                return OK;
            }
        }
        if (received < guppy_size || memcmp(g_short_request_message, guppy, guppy_size) != 0)
        {
            /* Rogue message, not worth creating a client. Specific to guppy because processor_fixed_response has no way to handle unsolicited messages */
            const char message[] = "4 No request\r\n";
            const size_t message_size = sizeof(message) - 1;
            (void)sendto(polls->p[index].fd, message, message_size, 0, (struct sockaddr*)&new_client.address, socket_address_size);
            return OK;
        }
        if (max_clients || max_memory || max_utilization)
        {
            server_send_low_resources_message(&new_client, polls->p[index].fd, max_clients, max_memory, max_utilization);
            return OK;
        }
        
        /* Create client */
        new_client.accepting_socket = index;
        new_client.last_request_complete = now;
        new_client.last_request_stream_not_empty = now;
        new_client.last_response_stream_not_full = now;
        new_client.last_response_complete = now;
        new_client.cryptography_state = CS_OPERATIONAL;
        PGOTO(polls_append(polls, &new_poll, 1));
        PGOTO(clients_append(clients, &new_client, 1));
        g_short_request_message_owner = &clients->p[clients->size - 1];
        g_short_request_message_size = received;
        return OK;
        
        failure:
        /* poll_finalize(&new_poll); */
        /* client_finalize(&new_client); */ /* Nothing is allocated */
        polls->size = old_size + ACCEPTING_SOCKETS;
        clients->size = old_size;
        return error;
    }
    else
    {
        ARET0(errno == EAGAIN || errno == EWOULDBLOCK, "recvfrom() failed");
    }
    return OK;
}

struct Error *server_accept_traffic(struct ClientBuffer *clients, struct PollBuffer *polls, double utilization, time_t now)
{
    const bool max_clients = clients->size >= MAX_CLIENTS;
    const bool max_memory = g_memory >= MAX_MEMORY_USAGE;
    const bool max_utilization = utilization >= MAX_PROCESSOR_UTILIZATION;
    unsigned char index;
    
    for (index = 0; index < ACCEPTING_SOCKETS; index++)
    {
        ARET0((polls->p[index].revents & (POLLERR | POLLHUP)) == 0, "listening socket failed");
        if ((polls->p[index].revents & POLLIN) == 0) continue;
        if (ACCEPTING_SOCKET_IS_CONNECTION(index))
        {
            PRET(server_accept_connection(clients, polls, now, index, max_clients, max_memory, max_utilization));
        }
        else if (g_short_request_message_owner == NULL) /* Only one message */
        {
            PRET(server_accept_message(clients, polls, now, index, max_clients, max_memory, max_utilization));
        }
    }
    return OK;
}
