#include "../include/error.h"
#include "../include/client.h"
#include "../include/path.h"
#include "../include/request_parser.h"
#include "../include/request_processor.h"
#include "../include/response_printer.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

static struct Error *create_listening_sockets(struct PollBuffer *polls) NODISCARD;
static struct Error *create_listening_sockets(struct PollBuffer *polls)
{
    struct Error *error = OK;
    struct pollfd new_poll = { -1, POLLIN, 0 };
    struct sockaddr_in socket_address = { 0 };

    new_poll.fd = socket(AF_INET, SOCK_STREAM, 0);
    AGOTO0(new_poll.fd >= 0, "socket() failed");

    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(8000);
    AGOTO0(bind(new_poll.fd, (struct sockaddr*)&socket_address, sizeof(socket_address)) >= 0, "bind() failed");
    AGOTO0(listen(new_poll.fd, 10) >= 0, "listed() failed");
    PGOTO(polls_append(polls, &new_poll, 1));
    return OK;

    failure:
    if (new_poll.fd >= 0) close(new_poll.fd);
    return error;
}

static struct Error *accept_new_connections(struct ClientBuffer *clients, struct PollBuffer *polls) NODISCARD;
static struct Error *accept_new_connections(struct ClientBuffer *clients, struct PollBuffer *polls)
{
    struct Error *error = OK;
    struct Client new_client = { 0 };
    struct pollfd new_poll = { -1, POLLIN | POLLHUP | POLLERR, 0 };
    socklen_t socket_address_size;

    if ((polls->p[0].revents & POLLIN) == 0) return OK;

    socket_address_size = sizeof(new_client.address);
    new_poll.fd = accept(polls->p[0].fd, (struct sockaddr*)&new_client.address, &socket_address_size);
    AGOTO0(new_poll.fd >= 0, "accept() failed"); /* TODO: not fatal */
    AGOTO0(fcntl(new_poll.fd, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed"); /* TODO: not fatal */
    PGOTO(clients_append(clients, &new_client, 1));
    PGOTO(polls_append(polls, &new_poll, 1));
    return OK;

    failure:
    if (new_poll.fd >= 0) close(new_poll.fd);
    return error;
}

static struct Error *server_main(int argc, char **argv) NODISCARD;
static struct Error *server_main(int argc, char **argv)
{
    struct Error *error = OK;
    struct ClientBuffer clients = { 0 };
    struct PollBuffer polls = { 0 };
    struct CharBuffer input_buffer = { 0 }, output_buffer = { 0 };
    struct Response intermediate_buffer = { 0 };
    struct Client *client_i, *surviving_client_i;
    struct pollfd *poll_i, *surviving_poll_i;
    AGOTO0(argc >= 0, "Not enough arguments");
    PGOTO(path_set_application(&g_application, argv[0]));

    /* Create socket */
    PGOTO(create_listening_sockets(&polls));
    
    /* Loop */
    while (true)
    {
        /* Waiting */
        int event_number = poll(polls.p, polls.size, -1);
        AGOTO0(event_number >= 0, "poll() failed");

        /*Accepting connections */
        PGOTO(accept_new_connections(&clients, &polls));

        /* Processing connections */
        for (surviving_client_i = client_i = clients.p, surviving_poll_i = poll_i = polls.p + 1; client_i < clients.p + clients.size; client_i++, poll_i++)
        {
            bool delete = false;
            if ((poll_i->revents & POLLIN) != 0)
            {
                /* Reading data */
                int available;
                ssize_t read_bytes;
                size_t buffer_used = 0;
                AGOTO0(ioctl(poll_i->fd, FIONREAD, &available) >= 0, "ioctl() failed"); /* TODO: not critical */
                PGOTO(char_buffer_resize(&input_buffer, (size_t)available));
                read_bytes = read(poll_i->fd, input_buffer.p, input_buffer.size);
                if (read_bytes == 0) delete = true;
                AGOTO0((size_t)read_bytes == input_buffer.size, "read() failed"); /* TODO: not critical */

                /* Processing data */
                output_buffer.size = 0;
                while (!delete)
                {
                    PGOTO(parse_request(&client_i->parser, &input_buffer, &buffer_used)); /* TODO: not critical */
                    if (client_i->parser.state != RPS_END) break;
                    PGOTO(process_request(&client_i->parser.request, &intermediate_buffer)); /* TODO: not critical, in fact it's notmal to fail */
                    PGOTO(print_response(&intermediate_buffer, &output_buffer)); /* TODO: not critical */
                    if (!client_i->parser.request.keep_alive) delete = true;
                    else parse_request_reset(&client_i->parser);
                }

                /* Sending data */
                if (output_buffer.size != 0)
                {
                    /* TODO: apparently send() may not send all bytes, throttling needed */
                    AGOTO0((size_t)send(poll_i->fd, output_buffer.p, output_buffer.size, 0) == output_buffer.size, "send() failed");
                }
            }
            else if ((poll_i->revents & POLLHUP) != 0 || (poll_i->revents & POLLERR) != 0)
            {
                /* Delete client */
                delete = true;
            }

            if (delete)
            {
                /* Deleting client */
                char_buffer_finalize(&client_i->parser.request.data);
                close(poll_i->fd);
            }
            else
            {
                /* Relocating client to its new place */
                if (surviving_client_i != client_i)
                {
                    *surviving_client_i = *client_i;
                    *surviving_poll_i = *poll_i;
                }
                surviving_client_i++;
                surviving_poll_i++;
            }
        }
        if (surviving_client_i != client_i)
        {
            PGOTO(clients_resize(&clients, (size_t)(surviving_client_i - clients.p)));
            PGOTO(polls_resize(&polls, (size_t)(surviving_poll_i - polls.p - 1)));
        }
    }

    failure:
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