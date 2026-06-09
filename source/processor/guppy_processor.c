#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/random.h"
#include "../../include/ring.h"
#include "../../include/utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRLF "\r\n"
#define ENDLINE "\r\n" /* Specification says CR is optional */

struct GuppyInsertMarkerTrace
{
    size_t total_size, payload_size, chunk_payload_size;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#pragma GCC diagnostic ignored "-Wanalyzer-out-of-bounds"
static struct Error *guppy_insert_markers(struct CharBuffer *buffer, unsigned int *first_chunk, unsigned int *chunk_number)
{
    /* TODO: optimize without allocation? */
    const size_t initial_payload_size = buffer->size;
    const size_t chunk_number_estimation = (initial_payload_size + CHUNK_SIZE - 1) / (CHUNK_SIZE - 32) + 1;
    struct GuppyInsertMarkerTrace *trace = malloc(chunk_number_estimation * sizeof(struct GuppyInsertMarkerTrace));
    struct GuppyInsertMarkerTrace *p = trace;
    unsigned int seq = 6 + (unsigned int)random_rand(0x1000000);
    size_t total_size = 0, payload_size = 0;
    char extra[64];
    ARET(trace != NULL);
    *first_chunk = seq;
    for (; ; seq++, p++)
    {
        const size_t chunk_extra_size = int_length(seq) + ((total_size == 0) ? 1 : 2);
        const size_t max_chunk_payload_size = CHUNK_SIZE - chunk_extra_size;
        const size_t chunk_payload_size = (initial_payload_size - payload_size < max_chunk_payload_size) ? (initial_payload_size - payload_size) : max_chunk_payload_size;
        const size_t chunk_total_size = chunk_extra_size + chunk_payload_size;
        p->total_size = total_size;
        p->payload_size = payload_size;
        p->chunk_payload_size = chunk_payload_size;
        payload_size += chunk_payload_size;
        total_size += chunk_total_size;
        if (chunk_payload_size == 0) break;
    }
    *chunk_number = seq - *first_chunk + 1;
    PRET(string_resize(buffer, total_size));
    for (; ; seq--, p--)
    {
        const size_t chunk_extra_size = (size_t)sprintf(extra, (seq == *first_chunk) ? "%u " : "%u" CRLF, seq);
        memmove(buffer->p + p->total_size + chunk_extra_size, buffer->p + p->payload_size, p->chunk_payload_size);
        memcpy(buffer->p + p->total_size, extra, chunk_extra_size);
        if (seq == *first_chunk) break;
    }
    if (trace != NULL) free(trace);
    return OK;
}
#pragma GCC diagnostic pop

struct Error *processor_print_guppy(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    switch (style)
    {
    case ES_INITIALIZE: /* Overload, no code */
        PRET(string_copy_mem(context->one, STRING_STRLEN("text/gemini" CRLF)));
        context->two->size = 0;
        break;

    case ES_FINALIZE: /* Overload, insert sequence numbers */
        PRET(guppy_insert_markers(context->one, &context->first_chunk, &context->chunk_number));
        break;

    case ES_INTERNAL_REFERENCE: /* Overload, different protocol */
        PRET(string_print_append(context->one, "=> guppy://" DOMAIN_NAME GUPPY_PORT_STRING "/%s ", resource));
        PRET(string_vprint_append(context->one, format, va));
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        break;

    default: /* Inherit */
        PRET(processor_print_gemini(context, style, resource, format, va));
        break;
    }
    return OK;
}

struct ExError processor_process_guppy(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;
    struct Value method, resource, protocol;
    
    *response_stream = zero;
    response_stream->parts[0].p = "4 Invalid resource" CRLF;
    response_stream->parts[0].size = sizeof("4 Invalid resource" CRLF) - 1;

    EXPRETF(ring_get(request_stream, &request->method, false, &method), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol), EEF_CLOSE_LOG_DIE);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &method, &resource, &protocol)); /* Just send it once */

    return EXOK;
}

struct ExError processor_fixed_guppy(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;

    processor_fixed_guppy_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_guppy_failsafe(enum FixedResponse fixed, struct Value *response_stream)
{
    const struct Value zero = ZERO_INIT;
    const char *fixed_string;
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    fixed_string = "4 Maximum number of clients reached" CRLF;      break;
    case FR_MAX_MEMORY:                     fixed_string = "4 Maximum memory usage reached" CRLF;           break;
    case FR_MAX_UTILIZATION:                fixed_string = "4 Maximum processor utilization reached" CRLF;  break;
    case FR_UNKNOWN:                        fixed_string = "4 Unknown error" CRLF;                          break;
    case FR_REQUEST_INVALID:                fixed_string = "4 Parser error" CRLF;                           break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   fixed_string = "4 Received too large chunk of data" CRLF;       break;
    case FR_MAX_REQUEST_HEADER_SIZE:        fixed_string = "4 Actual header size is too large" CRLF;        break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       fixed_string = "4 Promised content size is too large" CRLF;     break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    fixed_string = "4 Request incomplete for too long" CRLF;        break;
    default:                                fixed_string = "4 Unknown error" CRLF;                          break;
    }
    *response_stream = zero;
    response_stream->parts[0].p = (char*)fixed_string;
    response_stream->parts[0].size = strlen(fixed_string);
}
