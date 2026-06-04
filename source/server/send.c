#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/output.h"
#include "../../include/ring.h"
#include "../../include/value.h"

/* Pops and prints metadata of the current response (Failure -> close, log, and die) */
static struct Error *server_pop_response(struct Client *client) NODISCARD;
static struct Error *server_pop_response(struct Client *client)
{
    struct Value method, resource;
    PRET(ring_pop(&client->response_queue, sizeof(struct Response)));
    if (client->response.phony) return OK; /* Phony */
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
    return OK;
}

/* Pops and prints as many responses and their metadata as necessary (Failure -> close, log, and die) */
static struct Error *server_pop_responses(struct Client *client, time_t now, size_t sent_stream_size) NODISCARD;
static struct Error *server_pop_responses(struct Client *client, time_t now, size_t sent_stream_size)
{
    while (sent_stream_size > 0)
    {
        if (sent_stream_size >= client->response.stream_size)
        {
            /* Sent */
            PRET(server_pop_response(client));
            client->last_response_complete = now;
            client->response_count--;
            sent_stream_size -= client->response.stream_size;
            client->response.stream_size = 0;
            if (client->response_queue.size > 0)
            {
                struct ValueLocation location; struct Value value;
                location.offset = 0;
                location.size = sizeof(struct Response);
                PRET(ring_get(&client->response_queue, &location, false, &value));
                value_read(&value, (char*)&client->response);
            }
        }
        else
        {
            /* Not sent */
            client->response.stream_size -= sent_stream_size;
            sent_stream_size = 0;
        }
    }
    return OK;
}

/* Sends short-term output buffer (Failure -> close and log) */
static struct ExError server_send_short_response_stream(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags) NODISCARD;
static struct ExError server_send_short_response_stream(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    size_t old_value_size, new_value_size, sent_stream_size;

    /* Send chunk of data */
    old_value_size = value_const_size(&g_short_response_stream);
    EXPRETF(server_send_value(&g_short_response_stream, fd, flags), EEF_CLOSE_LOG);
    new_value_size = value_const_size(&g_short_response_stream);
    sent_stream_size = old_value_size - new_value_size;
    if (new_value_size == 0)
    {
        g_short_response_stream_owner = NULL;
        processor_free();
    }

    /* Analyze what was in that chunk of data */
    EXPRETF(server_pop_responses(client, now, sent_stream_size), EEF_CLOSE_LOG_DIE);
    /* TODO: additional sanity checks? */
    return EXOK;
}

/* Retrieves long-term output buffer */
static struct ExError server_send_long_output_buffer(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags) NODISCARD;
static struct ExError server_send_long_output_buffer(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    struct ConstValue value; struct Value nonconst_value;
    size_t old_value_size, new_value_size, sent_stream_size;
    
    /* Send chunk of data */
    ring_get_all(&client->response_stream, &nonconst_value);
    value_to_const_value(&value, &nonconst_value);
    old_value_size = value_const_size(&value);
    EXPRETF(server_send_value(&value, fd, flags), EEF_CLOSE_LOG);
    new_value_size = value_const_size(&value);
    sent_stream_size = old_value_size - new_value_size;
    EXPRETF(ring_pop(&client->response_stream, sent_stream_size), EEF_CLOSE_LOG_DIE);

    /* Analyze what was in that chunk of data */
    EXPRETF(server_pop_responses(client, now, sent_stream_size), EEF_CLOSE_LOG_DIE);
    /* TODO: additional sanity checks? */
    return EXOK;
}

struct ExError server_send_data(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    size_t short_response_stream_size = 0;
    if (client == g_short_response_stream_owner)
    {
        EXPRET(server_send_short_response_stream(client, fd, now, flags));
        short_response_stream_size = value_const_size(&g_short_response_stream);
    }
    if ((*flags & CF_SATURATED) == 0 && client->response_stream.size > 0)
    {
        EXPRET(server_send_long_output_buffer(client, fd, now, flags));
    }
    if (short_response_stream_size + client->response_stream.size < MAX_RESPONSE_STREAM_SIZE)
    {
        client->last_response_stream_not_full = now;
    }
    return EXOK;
}
