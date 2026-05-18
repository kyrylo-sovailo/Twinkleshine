#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/output.h"
#include "../../include/ring.h"
#include "../../include/value.h"

/* Prints and pops response metadata from response_queue (Failure -> close, log, and die) */
static struct Error *client_pop_response(struct Client *client)
{
    struct Value method, resource;
    PRET(ring_get(&client->response_queue, &client->response.method, false, &method));
    PRET(ring_get(&client->response_queue, &client->response.resource, false, &resource));
    output_open(false);
    output_print(false, "Success %.*s%.*s %.*s%.*s\n",
        (int)method.parts[0].size, method.parts[0].p, (int)method.parts[1].size, method.parts[1].p,
        (int)resource.parts[0].size, resource.parts[0].p, (int)resource.parts[1].size, resource.parts[1].p);
    output_print_client(false, client);
    output_print_time(true);
    output_print(true, COMMON_N);
    output_close(false);
    PRET(ring_pop(&client->response_queue, client->response.size));
    ARET(client->response_queue.size == 0);
    return OK;
}

/* Sends short-term output buffer (Failure -> close and log) */
static struct ExError client_send_short_response_stream(struct Client *client, int fd, time_t now, bool *connection_saturated) NODISCARD;
static struct ExError client_send_short_response_stream(struct Client *client, int fd, time_t now, bool *connection_saturated)
{
    const struct ExError EXOK = { OK };

    /* Send chunk of data */
    EXPRETF(server_send_value(&g_short_response_stream, fd, connection_saturated), EEF_CLOSE_LOG);

    /* See if it was the chunk was last */
    if (value_const_size(&g_short_response_stream) == 0)
    {
        g_short_response_stream_owner = NULL;
        processor_free();
        EXPRETF(client_pop_response(client), EEF_CLOSE_LOG_DIE);
        client->last_response_complete = now;
    }
    return EXOK;
}

/* Retrieves long-term output buffer */
static struct ExError client_send_long_output_buffer(struct Client *client, int fd, time_t now, bool *connection_saturated) NODISCARD;
static struct ExError client_send_long_output_buffer(struct Client *client, int fd, time_t now, bool *connection_saturated)
{
    const struct ExError EXOK = { OK };
    struct ConstValue value; struct Value nonconst_value;
    size_t old_value_size, new_value_size, sent_stream_size;
    
    /* Send chunk of data */
    ring_get_all(&client->response_stream, &nonconst_value);
    value_to_const_value(&value, &nonconst_value);
    old_value_size = value_const_size(&value);
    EXPRETF(server_send_value(&value, fd, connection_saturated), EEF_CLOSE_LOG);
    new_value_size = value_const_size(&value);
    sent_stream_size = old_value_size - new_value_size;
    EXPRETF(ring_pop(&client->response_stream, sent_stream_size), EEF_CLOSE_LOG_DIE);

    /* Analyze what was in that chunk of data */
    while (sent_stream_size > 0)
    {
        if (sent_stream_size >= client->response.stream_size)
        {
            /* Sent */
            EXPRETF(client_pop_response(client), EEF_CLOSE_LOG_DIE);
            client->last_response_complete = now;
            sent_stream_size -= client->response.stream_size;
            EXARET(client->response_queue.size == 0 || client->response_queue.size >= sizeof(struct Response), EEF_CLOSE_LOG_DIE);
            if (client->response_queue.size > 0)
            {
                EXPRETF(ring_pop_read(&client->response_queue, sizeof(struct Response), (char*)&client->response), EEF_CLOSE_LOG_DIE);
            }
        }
        else
        {
            /* Not sent */
            client->response.stream_size -= sent_stream_size;
            sent_stream_size = 0;
        }
    }
    return EXOK;
}

struct ExError server_send_data(struct Client *client, int fd, time_t now, bool *connection_saturated)
{
    const struct ExError EXOK = { OK };
    size_t short_response_stream_size = 0;
    if (client == g_short_response_stream_owner)
    {
        EXPRET(client_send_short_response_stream(client, fd, now, connection_saturated));
        short_response_stream_size = value_const_size(&g_short_response_stream);
    }
    if (!*connection_saturated && client->response_stream.size > 0)
    {
        EXPRET(client_send_long_output_buffer(client, fd, now, connection_saturated));
    }
    if (short_response_stream_size + client->response_stream.size < MAX_RESPONSE_STREAM_SIZE)
    {
        client->last_response_stream_not_full = now;
    }
    return EXOK;
}
