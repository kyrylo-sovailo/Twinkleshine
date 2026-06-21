#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/language.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

#define ENDLINE "\r\n" /* Specification says CRLF */

struct ExError processor_process_finger(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;
    union Value method, resource, protocol;
    
    *response_stream = zero;
    response_stream->parts[0].p = "Invalid resource" ENDLINE;
    response_stream->parts[0].size = strlen(response_stream->parts[0].p);

    EXPRETF(ring_get(request_stream, &request->method, false, &method.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol.m), EEF_CLOSE_LOG_DIE);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &method.c, &resource.c, &protocol.c));

    return EXOK;
}

struct ExError processor_fixed_finger(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;

    processor_fixed_finger_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_finger_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream)
{
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    response_stream->p = "Maximum number of clients reached" ENDLINE;       break;
    case FR_MAX_MEMORY:                     response_stream->p = "Maximum memory usage reached" ENDLINE;            break;
    case FR_MAX_UTILIZATION:                response_stream->p = "Maximum processor utilization reached" ENDLINE;   break;
    case FR_UNKNOWN:                        response_stream->p = "Unknown error" ENDLINE;                           break;
    case FR_REQUEST_INVALID:                response_stream->p = "Parser error" ENDLINE;                            break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   response_stream->p = "Received too large chunk of data" ENDLINE;        break;
    case FR_MAX_REQUEST_HEADER_SIZE:        response_stream->p = "Actual header size is too large" ENDLINE;         break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       response_stream->p = "Promised content size is too large" ENDLINE;      break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    response_stream->p = "Request incomplete for too long" ENDLINE;         break;
    default:                                response_stream->p = "Unknown error" ENDLINE;                           break;
    }
    response_stream->size = strlen(response_stream->p);
}


struct Error *processor_print_finger(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    /* Pre-prefix */
    size_t old_size;
    if ((processor_generic_paragraph(style) || processor_generic_paragraph(context->previous_style)) && style != ES_FINALIZE && context->previous_style != ES_INITIALIZE)
    {
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
    }
    old_size = context->one->size;
    
    /* Prefix */
    switch (style)
    {
    case ES_INITIALIZE: context->one->size = 0; context->two->size = 0; return OK;
    case ES_FINALIZE: return OK;
    case ES_ITEMIZE: PRET(string_append_mem(context->one, STRING_STRLEN(" - "))); break;
    case ES_ENUMERATION: PRET(string_print_append(context->one, " %u. ", context->list_index)); break;
    case ES_QUOTE: PRET(string_append_mem(context->one, STRING_STRLEN(" > "))); break;
    case ES_LARGEST:
    case ES_HEADER: PRET(string_append_mem(context->one, STRING_STRLEN("# "))); break;
    default: break;
    }
    
    /* Text */
    PRET(string_vprint_append(context->one, format, va));

    /* Suffix */
    switch (style)
    {
    case ES_LARGE:
    case ES_LARGER:
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        processor_generic_upper_case(context->one, old_size, sizeof("# ")-1, sizeof(ENDLINE)-1);
        break;
    case ES_LARGEST:
    case ES_HEADER:
        PRET(string_append_mem(context->one, STRING_STRLEN(" #" ENDLINE)));
        processor_generic_upper_case(context->one, old_size, sizeof("# ")-1, sizeof(ENDLINE)-1);
        PRET(processor_generic_box(context->one, old_size, sizeof("# ")-1, sizeof(ENDLINE)-1, '#', '#'));
        break;
    case ES_INTERNAL_REFERENCE:
        PRET(string_print_append(context->one, ": finger://" DOMAIN_NAME FINGER_PORT_STRING "/%s%s" ENDLINE,
            resource, language_slash(context->language, context->requested_language, context->request->language)));
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
