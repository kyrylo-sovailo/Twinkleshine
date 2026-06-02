#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/parser.h"
#include "../../include/random.h"
#include "../../include/ring.h"
#include "../../include/tables.h"

#include <stdbool.h>
#include <string.h>

#define CRLF "\r\n"

static const char gemini_template_index_page[] =
"20 text/gemini" CRLF
"# This is Kyrylo's website." CRLF
"It is displaying correctly." CRLF
"The style is not a bug. It is a feature." CRLF
"Served to you by the Twinkleshine server, fully written in C by yours truly." CRLF
"=> https://github.com/kyrylo-sovailo/Twinkleshine Twinkleshine" CRLF
"Fun fact: %s" CRLF;

static void processor_set_locations_one(const struct ValueLocation *request_location, struct ValueLocation *response_location, size_t *size)
{
    response_location->offset = *size;
    response_location->size = request_location->size;
    *size += request_location->size;
}

static void processor_set_locations(const struct Request *request, struct Response *response)
{
    const struct ValueLocation zero = ZERO_INIT;
    response->size = 0;
    response->method = zero;
    response->protocol = zero;
    processor_set_locations_one(&request->resource, &response->resource, &response->size);
}

static struct Error *processor_push_metadata_one(const struct ValueLocation *request_location, const struct Ring *request_stream, struct Ring *response_queue) NODISCARD;
static struct Error *processor_push_metadata_one(const struct ValueLocation *request_location, const struct Ring *request_stream, struct Ring *response_queue)
{
    struct Value value;
    unsigned char i;
    
    PRET(ring_get(request_stream, request_location, false, &value));
    for (i = 0; i < VALUE_PARTS; i++)
    {
        PRET(ring_push_write(response_queue, value.parts[i].size, value.parts[i].p));
    }
    return OK;
}

static struct Error *processor_push_metadata(const struct Request *request, const struct Ring *request_stream, struct Ring *response_queue) NODISCARD;
static struct Error *processor_push_metadata(const struct Request *request, const struct Ring *request_stream, struct Ring *response_queue)
{
    PRET(processor_push_metadata_one(&request->resource, request_stream, response_queue));
    return OK;
}

struct ExError processor_process_gemini(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstValue zero = ZERO_INIT;
    struct Value resource;
    
    /* Actual logic */
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
    if (value_compare_case_mem(&resource, "", strlen("")))
    {
        EXPRETF(string_print(&g_internal_buffer_one, gemini_template_index_page, fun_facts[random_rand(sizeof(fun_facts)/sizeof(*fun_facts))]), EEF_CLOSE_LOG);
        *response_stream = zero;
        response_stream->parts[0].p = g_internal_buffer_one.p;
        response_stream->parts[0].size = g_internal_buffer_one.size;
    }
    else
    {
        *response_stream = zero;
        response_stream->parts[0].p = "40 Invalid resource" CRLF;
        response_stream->parts[0].size = strlen(response_stream->parts[0].p);
    }

    /* Response */
    processor_set_locations(request, response);
    response->keep_alive = false;
    response->stream_size = g_internal_buffer_one.size;

    /* Metadata */
    EXPRETF(ring_reserve(response_queue, response_queue->size + sizeof(struct Response) + response->size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (const char*)response), EEF_CLOSE_LOG_DIE);
    EXPRETF(processor_push_metadata(request, request_stream, response_queue), EEF_CLOSE_LOG_DIE);

    return EXOK;
}

struct ExError processor_fixed_gemini(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Response zero = ZERO_INIT;

    /* Actual logic */
    processor_fixed_gemini_failsafe(fixed, response_stream);

    /* Response */
    *response = zero;
    response->keep_alive = false;
    response->stream_size = value_const_size(response_stream);

    /* Metadata */
    EXPRETF(ring_reserve(response_queue, response_queue->size + sizeof(struct Response) + response->size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (const char*)response), EEF_CLOSE_LOG_DIE);
    /* TODO: some metadata? */

    return EXOK;
}

void processor_fixed_gemini_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream)
{
    const struct ConstValue zero = ZERO_INIT;
    const char *fixed_string;
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    fixed_string = "41 Maximum number of clients reached" CRLF;     break;
    case FR_MAX_MEMORY:                     fixed_string = "41 Maximum memory usage reached" CRLF;          break;
    case FR_MAX_UTILIZATION:                fixed_string = "41 Maximum processor utilization reached" CRLF; break;
    case FR_UNKNOWN:                        fixed_string = "42 Unknown error" CRLF;                         break;
    case FR_REQUEST_INVALID:                fixed_string = "59 Parser error" CRLF;                          break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   fixed_string = "59 Received too large chunk of data" CRLF;      break;
    case FR_MAX_REQUEST_HEADER_SIZE:        fixed_string = "59 Actual header size is too large" CRLF;       break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       fixed_string = "59 Promised content size is too large" CRLF;    break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    fixed_string = "59 Request incomplete for too long" CRLF;       break;
    default:                                fixed_string = "42 Unknown error" CRLF;                         break;
    }
    *response_stream = zero;
    response_stream->parts[0].p = fixed_string;
    response_stream->parts[0].size = strlen(fixed_string);
}
