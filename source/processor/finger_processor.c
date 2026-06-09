#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

#define ENDLINE "\r\n" /* Specification says CRLF */

struct Error *processor_print_finger(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    size_t old_size, line_size;
    char *p;
    
    /* Prefix */
    context->list_index = (style == ES_ENUMERATION) ? (context->list_index + 1) : 0;
    switch (style)
    {
    case ES_INITIALIZE: context->one->size = 0; context->two->size = 0; return OK;
    case ES_FINALIZE: return OK;
    case ES_ITEMIZE: PRET(string_append_mem(context->one, STRING_STRLEN(" - "))); break;
    case ES_ENUMERATION: PRET(string_print_append(context->one, " %u. ", context->list_index)); break;
    case ES_QUOTE: PRET(string_append_mem(context->one, STRING_STRLEN(" > "))); break;
    case ES_LARGE:
    case ES_LARGER:
        if (context->one->size > 0) PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        old_size = context->one->size;
        break;
    case ES_LARGEST:
    case ES_HEADER:
        if (context->one->size > 0) PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        old_size = context->one->size; PRET(string_append_mem(context->one, STRING_STRLEN("# ")));
        break;
    default: break;
    }
    
    /* Text */
    PRET(string_vprint_append(context->one, format, va));

    /* Suffix */
    switch (style)
    {
    case ES_LARGE:
    case ES_LARGER:
        for (p = context->one->p + old_size; p < context->one->p + context->one->size; p++) *p = (*p >= 'a' && *p <= 'z') ? (*p - 'a' + 'A') : *p;
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        break;
    case ES_LARGEST:
    case ES_HEADER:
        for (p = context->one->p + old_size; p < context->one->p + context->one->size; p++) *p = (*p >= 'a' && *p <= 'z') ? (*p - 'a' + 'A') : *p;
        PRET(string_append_mem(context->one, STRING_STRLEN(" #" ENDLINE)));
        line_size = context->one->size - old_size;
        PRET(string_print_append(context->one, "%*s", (int)line_size * 2, ""));
        memcpy(context->one->p + old_size + line_size    , context->one->p + old_size, line_size);
        memcpy(context->one->p + old_size + line_size * 2, context->one->p + old_size, line_size);
        memset(context->one->p + old_size                , '#', line_size - (sizeof(ENDLINE)-1));
        memset(context->one->p + old_size + line_size * 2, '#', line_size - (sizeof(ENDLINE)-1));
        break;
    case ES_INTERNAL_REFERENCE:
        PRET(string_print_append(context->one, ": finger://" DOMAIN_NAME FINGER_PORT_STRING "/%s" ENDLINE, resource)); break;
        break;
    case ES_EXTERNAL_REFERENCE:
        PRET(string_print_append(context->one, ": %s" ENDLINE, resource));
        break;
    default:
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        break;
    }
    return OK;
}

struct ExError processor_process_finger(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;
    struct Value method, resource, protocol;
    
    *response_stream = zero;
    response_stream->parts[0].p = "Invalid resource" ENDLINE;
    response_stream->parts[0].size = sizeof("Invalid resource" ENDLINE) - 1;

    EXPRETF(ring_get(request_stream, &request->method, false, &method), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol), EEF_CLOSE_LOG_DIE);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &method, &resource, &protocol));

    return EXOK;
}

struct ExError processor_fixed_finger(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;

    processor_fixed_finger_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_finger_failsafe(enum FixedResponse fixed, struct Value *response_stream)
{
    const struct Value zero = ZERO_INIT;
    const char *fixed_string;
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    fixed_string = "Maximum number of clients reached" ENDLINE;     break;
    case FR_MAX_MEMORY:                     fixed_string = "Maximum memory usage reached" ENDLINE;          break;
    case FR_MAX_UTILIZATION:                fixed_string = "Maximum processor utilization reached" ENDLINE; break;
    case FR_UNKNOWN:                        fixed_string = "Unknown error" ENDLINE;                         break;
    case FR_REQUEST_INVALID:                fixed_string = "Parser error" ENDLINE;                          break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   fixed_string = "Received too large chunk of data" ENDLINE;      break;
    case FR_MAX_REQUEST_HEADER_SIZE:        fixed_string = "Actual header size is too large" ENDLINE;       break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       fixed_string = "Promised content size is too large" ENDLINE;    break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    fixed_string = "Request incomplete for too long" ENDLINE;       break;
    default:                                fixed_string = "Unknown error" ENDLINE;                         break;
    }
    *response_stream = zero;
    response_stream->parts[0].p = (char*)fixed_string; /* TODO: do something with const values */
    response_stream->parts[0].size = strlen(fixed_string);
}
