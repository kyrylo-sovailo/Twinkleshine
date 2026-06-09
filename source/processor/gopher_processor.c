#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/macro.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

#define FAKE "\tfake\t(NULL)\t0\r\n"
#define ENDLINE "\r\n" /* Specification says CRLF */
#define FAKE_ENDLINE FAKE ENDLINE

static void memset_alt(char *destination, size_t n)
{
    /* Lagrange can't handle long sequences of equal symbols */
    size_t i;
    for (i = 0; i < n; i++) destination[i] = (i & 1) ? '0' : 'O';
}

struct Error *processor_print_gopher(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    size_t old_size, line_size;
    char *p;
    
    /* Prefix */
    context->list_index = (style == ES_ENUMERATION) ? (context->list_index + 1) : 0;
    switch (style)
    {
    case ES_INITIALIZE: context->one->size = 0; context->two->size = 0; return OK;
    case ES_FINALIZE: return OK;
    case ES_ITEMIZE: PRET(string_append_mem(context->one, STRING_STRLEN("i - "))); break;
    case ES_ENUMERATION: PRET(string_print_append(context->one, "i %u. ", context->list_index)); break;
    case ES_QUOTE: PRET(string_append_mem(context->one, STRING_STRLEN("i > "))); break;
    case ES_LARGE:
    case ES_LARGER:
        if (context->one->size > 0) PRET(string_append_mem(context->one, STRING_STRLEN("i" FAKE_ENDLINE)));
        old_size = context->one->size;
        PRET(string_append_mem(context->one, STRING_STRLEN("i")));
        break;
    case ES_LARGEST:
    case ES_HEADER:
        if (context->one->size > 0) PRET(string_append_mem(context->one, STRING_STRLEN("i" FAKE_ENDLINE)));
        old_size = context->one->size;
        PRET(string_append_mem(context->one, STRING_STRLEN("iO ")));
        break;
    case ES_INTERNAL_REFERENCE:
        PRET(string_print_append(context->one, "1"));
        break;
    default:
        PRET(string_append_mem(context->one, STRING_STRLEN("i")));
        break;
    }
    
    /* Text */
    PRET(string_vprint_append(context->one, format, va));

    /* Suffix */
    switch (style)
    {
    case ES_LARGE:
    case ES_LARGER:
        for (p = context->one->p + old_size + 1; p < context->one->p + context->one->size; p++) *p = (*p >= 'a' && *p <= 'z') ? (*p - 'a' + 'A') : *p;
        PRET(string_append_mem(context->one, STRING_STRLEN(FAKE_ENDLINE)));
        break;
    case ES_LARGEST:
    case ES_HEADER:
        for (p = context->one->p + old_size + 1; p < context->one->p + context->one->size; p++) *p = (*p >= 'a' && *p <= 'z') ? (*p - 'a' + 'A') : *p;
        PRET(string_append_mem(context->one, STRING_STRLEN(" O"FAKE_ENDLINE)));
        line_size = context->one->size - old_size;
        PRET(string_print_append(context->one, "%*s", (int)line_size * 2, ""));
        memcpy(context->one->p + old_size + line_size    , context->one->p + old_size, line_size);
        memcpy(context->one->p + old_size + line_size * 2, context->one->p + old_size, line_size);
        memset_alt(context->one->p + old_size                 + 1, line_size - 1 - (sizeof(FAKE_ENDLINE)-1));
        memset_alt(context->one->p + old_size + line_size * 2 + 1, line_size - 1 - (sizeof(FAKE_ENDLINE)-1));
        break;
    case ES_INTERNAL_REFERENCE:
        PRET(string_print_append(context->one, "\t%s\t" DOMAIN_NAME "\t" STRINGIZE(GOPHER_PORT) ENDLINE, resource));
        break;
    case ES_EXTERNAL_REFERENCE:
        PRET(string_print_append(context->one, ": %s" FAKE_ENDLINE, resource));
        break;
    default:
        PRET(string_append_mem(context->one, STRING_STRLEN(FAKE_ENDLINE))); break;
        break;
    }
    return OK;
}

struct ExError processor_process_gopher(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;
    struct Value method, resource, protocol;
    
    *response_stream = zero;
    response_stream->parts[0].p = "3Invalid resource" FAKE_ENDLINE;
    response_stream->parts[0].size = sizeof("3Invalid resource" FAKE_ENDLINE) - 1;

    EXPRETF(ring_get(request_stream, &request->method, false, &method), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol), EEF_CLOSE_LOG_DIE);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &method, &resource, &protocol));

    return EXOK;
}

struct ExError processor_fixed_gopher(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;

    processor_fixed_gopher_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_gopher_failsafe(enum FixedResponse fixed, struct Value *response_stream)
{
    const struct Value zero = ZERO_INIT;
    const char *fixed_string;
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    fixed_string = "3Maximum number of clients reached" FAKE_ENDLINE;       break;
    case FR_MAX_MEMORY:                     fixed_string = "3Maximum memory usage reached" FAKE_ENDLINE;            break;
    case FR_MAX_UTILIZATION:                fixed_string = "3Maximum processor utilization reached" FAKE_ENDLINE;   break;
    case FR_UNKNOWN:                        fixed_string = "3Unknown error" FAKE_ENDLINE;                           break;
    case FR_REQUEST_INVALID:                fixed_string = "3Parser error" FAKE_ENDLINE;                            break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   fixed_string = "3Received too large chunk of data" FAKE_ENDLINE;        break;
    case FR_MAX_REQUEST_HEADER_SIZE:        fixed_string = "3Actual header size is too large" FAKE_ENDLINE;         break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       fixed_string = "3Promised content size is too large" FAKE_ENDLINE;      break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    fixed_string = "3Request incomplete for too long" FAKE_ENDLINE;         break;
    default:                                fixed_string = "3Unknown error" FAKE_ENDLINE;                           break;
    }
    *response_stream = zero;
    response_stream->parts[0].p = (char*)fixed_string;
    response_stream->parts[0].size = strlen(fixed_string);
}
