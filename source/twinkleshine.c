#include "../include/error.h"
#include "../include/client.h"
#include "../include/request_parser.h"
#include "../include/request_processor.h"
#include "../include/response_printer.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
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
    struct CharBuffer input_buffer = { 0 }, output_buffer = { 0 };
    struct Response intermediate_buffer = { 0 };
    struct ClientBuffer clients = { 0 };
    struct PollBuffer polls = { 0 };
    struct Client *client_i, *surviving_client_i;
    struct pollfd *poll_i, *surviving_poll_i;
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
            socket_address_size = sizeof(client_i->address);
            PGOTO(clients_resize(&clients, clients.size + 1));
            client_i = &clients.p[clients.size - 1];
            memset(client_i, 0, sizeof(*clients.p));
            PGOTO(polls_resize(&polls, polls.size + 1));
            poll_i = &polls.p[polls.size - 1];
            poll_i->events = POLLIN | POLLHUP | POLLERR;
            poll_i->revents = 0;
            poll_i->fd = accept(polls.p[0].fd, (struct sockaddr*)&client_i->address, &socket_address_size);
            AGOTO0(poll_i->fd >= 0, "accept() failed"); /* TODO: not critical */
            AGOTO0(fcntl(poll_i->fd, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed");
        }

        /* Processing connections */
        for (surviving_client_i = client_i = clients.p, surviving_poll_i = poll_i = polls.p + 1; client_i < clients.p + clients.size; client_i++, poll_i++)
        {
            if ((poll_i->revents & POLLIN) != 0)
            {
                /* Reading data */
                int available;
                ssize_t read_bytes;
                size_t buffer_used = 0;
                AGOTO0(ioctl(poll_i->fd, FIONREAD, &available), "ioctl() failed"); /* TODO: not critical */
                PGOTO(char_buffer_resize(&input_buffer, (size_t)available));
                read_bytes = read(poll_i->fd, input_buffer.p, input_buffer.size);
                AGOTO0((size_t)read_bytes == input_buffer.size, "read() failed"); /* TODO: not critical */

                /* Processing data */
                output_buffer.size = 0;
                while (true)
                {
                    PGOTO(parse_request(&client_i->parser, &input_buffer, &buffer_used)); /* TODO: not critical */
                    PGOTO(process_request(&client_i->parser.request, &intermediate_buffer)); /* TODO: not critical, in fact it's notmal to fail */
                    PGOTO(print_response(&intermediate_buffer, &output_buffer)); /* TODO: not critical */
                    parse_request_reset(&client_i->parser);
                }

                /* Sending data */
                AGOTO0((size_t)send(poll_i->fd, output_buffer.p, output_buffer.size, 0) == output_buffer.size, "send() failed");
                /* TODO: apparently send() may not send all bytes, throttling needed */

                /* Relocating client */
                if (surviving_client_i != client_i)
                {
                    *surviving_client_i = *client_i;
                    *surviving_poll_i = *poll_i;
                    surviving_client_i++;
                    surviving_poll_i++;
                }
            }
            else if ((poll_i->revents & POLLHUP) != 0 || (poll_i->revents & POLLERR) != 0)
            {
                /* Delete client */
                char_buffer_finalize(&client_i->parser.request.data);
                close(poll_i->fd);
            }
        }
        if (surviving_client_i != client_i)
        {
            PGOTO(clients_resize(&clients, (size_t)(surviving_client_i - clients.p)));
            PGOTO(polls_resize(&polls, (size_t)(surviving_poll_i - polls.p - 1)));
        }
    }

    finalize:
    for (client_i = clients.p; client_i < clients.p + clients.size; client_i++) char_buffer_finalize(&client_i->parser.request.data);
    for (poll_i = polls.p; poll_i < polls.p + polls.size; poll_i++) { if (poll_i->fd != -1) close(poll_i->fd); }
    clients_finalize(&clients);
    polls_finalize(&polls);
    char_buffer_finalize(&input_buffer);
    char_buffer_finalize(&intermediate_buffer.content);
    char_buffer_finalize(&output_buffer);
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