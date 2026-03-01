#include "../include/error.h"
#include "../include/client.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string.h>

static struct Error *server_main(int argc, char **argv) NODISCARD;
static struct Error *server_main(int argc, char **argv)
{
    struct Error *error = OK;
    struct sockaddr_in socket_address = { 0 };
    socklen_t socket_address_size;
    struct ClientBuffer clients = { 0 };
    struct PollBuffer polls = { 0 };
    struct Client *client_i;
    struct pollfd *poll_i;
    (void)argc;
    (void)argv;

    /* Create socket */
    PGOTO(polls_resize(&polls, 1));
    polls.p[0].events = POLLIN;
    polls.p[0].fd = socket(AF_INET, SOCK_STREAM, 0);
    AGOTO0(polls.p[0].fd >= 0, "socket() failed");
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(8000);
    socket_address_size = sizeof(socket_address);
    AGOTO0(bind(polls.p[0].fd, (struct sockaddr*)&socket_address, socket_address_size) >= 0, "bind() failed");
    AGOTO0(listen(polls.p[0].fd, 10) >= 0, "listed() failed");
    
    /* Loop */
    while (true)
    {
        int event_number = poll(polls.p, polls.size, -1);
        AGOTO0(event_number >= 0, "poll() failed");

        /* Accepting new connections */
        if ((polls.p[0].revents & POLLIN) != 0)
        {
            socket_address_size = sizeof(socket_address);
            PGOTO(clients_resize(&clients, clients.size + 1));
            client_i = &clients.p[clients.size - 1];
            memset(client_i, 0, sizeof(*clients.p));
            PGOTO(polls_resize(&polls, polls.size + 1));
            poll_i = &polls.p[polls.size - 1];
            memset(poll_i, 0, sizeof(*polls.p));
            poll_i->events = POLLIN | POLLHUP | POLLERR;
            poll_i->revents = 0;
            poll_i->fd = accept(polls.p[0].fd, (struct sockaddr*)&socket_address, &socket_address_size);
            AGOTO0(poll_i->fd >= 0, "accept() failed"); /* TODO: not critical */
            AGOTO0(fcntl(poll_i->fd, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed");
        }

        /* Processing connections */
        for (client_i = clients.p, poll_i = polls.p + 1; client_i < clients.p + clients.size; client_i++, poll_i++)
        {
            if ((poll_i->revents & POLLIN) != 0)
            {
                /* Reading data */
                int available, read;
                AGOTO0(ioctl(poll_i->fd, FIONREAD, &available), "ioctl() failed"); /* TODO: not critical */
                int n = read(poll_i->fd, buf, sizeof(buf)); /* TODO: not critical */
            }
            else if ((poll_i->revents & POLLHUP) != 0 || (poll_i->revents & POLLERR) != 0)
            {}
        }
    }

    finalize:
    for (client_i = clients.p; client_i < clients.p + clients.size; client_i++) char_buffer_finalize(&client_i->parser.request.data);
    for (poll_i = polls.p; poll_i < polls.p + polls.size; poll_i++) { if (poll_i->fd != -1) close(poll_i->fd); }
    clients_finalize(&clients);
    polls_finalize(&polls);
    return error;
}

int main(int argc, char **argv)
{
    int code = 0;
    struct Error *error = server_main(argc, argv);
    if (error != OK)
    {
        error_print(error);
        code = error_get_exit_code(error);
        error_finalize(error);
    }
    return code;
}