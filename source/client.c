#include "../include/client.h"
#include "../include/random.h"
#include "../include/request_processor.h"
#include "../commonlib/include/buffer_implementation.h"

#include <unistd.h>

void clients_shuffle(struct ClientBuffer *clients)
{
    /* Fisher-Yates shuffle */
    size_t i;
    for (i = 0; i < clients->size; i++) clients->p[i].shuffle_index = i;
    if (clients->size == 0) return;
    for (i = clients->size - 1; i >= 1; i--)
    {
        size_t j, shuffle_index;
        struct Client *client_i, *client_j;
        j = fair_rand(i + 1);
        client_i = &clients->p[i];
        client_j = &clients->p[j];
        shuffle_index = client_i->shuffle_index;
        client_i->shuffle_index = client_j->shuffle_index;
        client_j->shuffle_index = shuffle_index;
    }
}

void clients_remove_finalized(struct ClientBuffer *clients, struct PollBuffer *polls)
{
    struct Client *client_i, *surviving_client_i;
    struct pollfd *poll_i, *surviving_poll_i;
    for (surviving_client_i = client_i = clients->p, surviving_poll_i = poll_i = polls->p + 1; client_i < clients->p + clients->size; client_i++, poll_i++)
    {
        if (poll_i->fd >= 0)
        {
            if (surviving_client_i != client_i)
            {
                *surviving_client_i = *client_i;
                *surviving_poll_i = *poll_i;
            }
            surviving_client_i++;
            surviving_poll_i++;
        }
    }
    clients->size -= (size_t)(client_i - surviving_client_i); /* This is correct, no resize, no finalizers called */
    polls->size -= (size_t)(poll_i - surviving_poll_i);
}

void client_finalize(struct Client *client)
{
    if (client->short_output_buffer.size > 0) { const struct ConstValuePart zero = ZERO_INIT; client->short_output_buffer = zero; request_processor_free(); }
    ring_finalize(&client->input_buffer);
    ring_finalize(&client->output_buffer);
}

void poll_finalize(struct pollfd *poll)
{
    if (poll->fd >= 0) { close(poll->fd); poll->fd = -1; }
}

static void clients_initialize_element(struct Client *client) { const struct Client zero = ZERO_INIT; *client = zero;  }
static void clients_finalize_element(struct Client *client) { client_finalize(client); }
static void polls_initialize_element(struct pollfd *poll) { const struct pollfd zero = { -1, 0, 0 }; *poll = zero;  }
static void polls_finalize_element(struct pollfd *poll) { poll_finalize(poll); }

IMPLEMENT_BUFFER_RESIZE(struct Client, ClientBuffer, clients_)
IMPLEMENT_BUFFER_APPEND(struct Client, ClientBuffer, clients_)
IMPLEMENT_BUFFER_FINALIZE(struct Client, ClientBuffer, clients_)

IMPLEMENT_BUFFER_RESIZE(struct pollfd, PollBuffer, polls_)
IMPLEMENT_BUFFER_APPEND(struct pollfd, PollBuffer, polls_)
IMPLEMENT_BUFFER_FINALIZE(struct pollfd, PollBuffer, polls_)
