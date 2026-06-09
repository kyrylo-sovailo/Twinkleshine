#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/output.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

static struct Error *server_try_socket_pair(struct PollBuffer *polls, unsigned short port, bool connection, bool try_ipv4, bool try_ipv6, bool try_ipv6_only) NODISCARD;
static struct Error *server_try_socket_pair(struct PollBuffer *polls, unsigned short port, bool connection, bool try_ipv4, bool try_ipv6, bool try_ipv6_only)
{
    const size_t old_size = polls->size;
    const int no_int = 0, yes_int = 1;
    struct Error *error = OK;
    struct pollfd ipv4_poll = { -1, POLLIN, 0 };
    struct pollfd ipv6_poll = { -1, POLLIN, 0 };

    if (try_ipv4)
    {
        int flags;
        struct sockaddr_in ipv4 = ZERO_INIT;
        ipv4.sin_family = AF_INET;
        ipv4.sin_addr.s_addr = INADDR_ANY;
        ipv4.sin_port = htons(port);
    
        ipv4_poll.fd = socket(AF_INET, connection ? SOCK_STREAM : SOCK_DGRAM, connection ? 0 : IPPROTO_UDP);
        if (ipv4_poll.fd < 0) goto failure;
        flags = fcntl(ipv4_poll.fd, F_GETFL, 0);
        if (flags < 0) goto failure;
        if (fcntl(ipv4_poll.fd, F_SETFL, flags | O_NONBLOCK) < 0) goto failure;
        if (setsockopt(ipv4_poll.fd, SOL_SOCKET, SO_REUSEADDR, &yes_int, sizeof(yes_int)) < 0) goto failure;
        if (bind(ipv4_poll.fd, (struct sockaddr*)&ipv4, sizeof(ipv4)) < 0) goto failure;
        if (connection)
        {
            if (listen(ipv4_poll.fd, MAX_CLIENTS_IN_QUEUE) < 0) goto failure;
        }
    }

    if (try_ipv6)
    {
        int flags;
        struct sockaddr_in6 ipv6 = ZERO_INIT;
        ipv6.sin6_family = AF_INET6;
        ipv6.sin6_addr = in6addr_any;
        ipv6.sin6_port = htons(port);

        ipv6_poll.fd = socket(AF_INET6, connection ? SOCK_STREAM : SOCK_DGRAM, connection ? 0 : IPPROTO_UDP);
        if (ipv6_poll.fd < 0) goto failure;
        flags = fcntl(ipv6_poll.fd, F_GETFL, 0);
        if (flags < 0) goto failure;
        if (fcntl(ipv6_poll.fd, F_SETFL, flags | O_NONBLOCK) < 0) goto failure;
        if (setsockopt(ipv6_poll.fd, SOL_SOCKET, SO_REUSEADDR, &yes_int, sizeof(yes_int)) < 0) goto failure;
        if (setsockopt(ipv6_poll.fd, IPPROTO_IPV6, IPV6_V6ONLY, try_ipv6_only ? &yes_int : &no_int, sizeof(int)) < 0) goto failure;
        if (bind(ipv6_poll.fd, (struct sockaddr*)&ipv6, sizeof(ipv6)) < 0) goto failure;
        if (connection)
        {
            if (listen(ipv6_poll.fd, MAX_CLIENTS_IN_QUEUE) < 0) goto failure;
        }
    }

    if (try_ipv4) { PGOTO(polls_append(polls, &ipv4_poll, 1)); }
    if (try_ipv6) { PGOTO(polls_append(polls, &ipv6_poll, 1)); }
    return OK;

    failure:
    if (ipv4_poll.fd >= 0) close(ipv4_poll.fd);
    if (ipv6_poll.fd >= 0) close(ipv6_poll.fd);
    polls->size = old_size;
    return error;
}

static struct Error *server_create_socket_pair(struct PollBuffer *polls, const char ***mode, unsigned short port, bool connection) NODISCARD;
static struct Error *server_create_socket_pair(struct PollBuffer *polls, const char ***mode, unsigned short port, bool connection)
{
    /* Trying in that order: IPv6+IPv4 > IPv6* > IPv6 > IPv4 > try again with reserve port */
    const size_t old_size = polls->size;
    PRET(server_try_socket_pair(polls, port, connection, true, true, true));
    if (polls->size > old_size) return OK;
    (*mode)++;
    PRET(server_try_socket_pair(polls, port, connection, false, true, false));
    if (polls->size > old_size) return OK;
    (*mode)++;
    PRET(server_try_socket_pair(polls, port, connection, false, true, true));
    if (polls->size > old_size) return OK;
    (*mode)++;
    PRET(server_try_socket_pair(polls, port, connection, true, false, false));
    if (polls->size > old_size) return OK;
    (*mode)++;
    RET0("All modes failed");
}

struct Error *server_initialize(struct PollBuffer *polls)
{
    const struct pollfd zero = { -1, POLLIN, 0 };
    const char *modes[] = { "IPv6+IPv4", "IPv6*", "IPv6", "IPv4" };
    const char **http_mode, **https_mode, **gopher_mode, **finger_mode, **gemini_mode, **spartan_mode, **nex_mode, **guppy_mode;

    ARET0(signal(SIGPIPE, SIG_IGN) != SIG_ERR, "signal() failed");
    
    http_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &http_mode, HTTP_PORT, true));
    for (;polls->size < 2;) { PRET(polls_append(polls, &zero, 1)); }

    https_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &https_mode, HTTPS_PORT, true));
    for (;polls->size < 4;) { PRET(polls_append(polls, &zero, 1)); }

    gopher_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &gopher_mode, GOPHER_PORT, true));
    for (;polls->size < 6;) { PRET(polls_append(polls, &zero, 1)); }

    finger_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &finger_mode, FINGER_PORT, true));
    for (;polls->size < 8;) { PRET(polls_append(polls, &zero, 1)); }

    gemini_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &gemini_mode, GEMINI_PORT, true));
    for (;polls->size < 10;) { PRET(polls_append(polls, &zero, 1)); }

    spartan_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &spartan_mode, SPARTAN_PORT, true));
    for (;polls->size < 12;) { PRET(polls_append(polls, &zero, 1)); }

    nex_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &nex_mode, NEX_PORT, true));
    for (;polls->size < 14;) { PRET(polls_append(polls, &zero, 1)); }

    guppy_mode = &modes[0];
    PRET(server_create_socket_pair(polls, &guppy_mode, GUPPY_PORT, false));
    for (;polls->size < 16;) { PRET(polls_append(polls, &zero, 1)); }

    output_open(false);
    output_print(false, "Twinkleshine started\n");
    output_print(false, "HTTP    mode: %s:%d\n", *http_mode, HTTP_PORT);
    output_print(false, "HTTPS   mode: %s:%d\n", *https_mode, HTTPS_PORT);
    output_print(false, "Gopher  mode: %s:%d\n", *gopher_mode, GOPHER_PORT);
    output_print(false, "Finger  mode: %s:%d\n", *finger_mode, FINGER_PORT);
    output_print(false, "Gemini  mode: %s:%d\n", *gemini_mode, GEMINI_PORT);
    output_print(false, "Spartan mode: %s:%d\n", *spartan_mode, SPARTAN_PORT);
    output_print(false, "Nex     mode: %s:%d\n", *nex_mode, NEX_PORT);
    output_print(false, "Guppy   mode: %s:%d\n", *guppy_mode, GUPPY_PORT);
    output_print_time(false);
    output_print(false, "\n");
    output_close(false);
    return OK;
}

void server_finalize(struct ClientBuffer *clients, struct PollBuffer *polls)
{
    clients_finalize(clients);
    polls_finalize(polls);
}
