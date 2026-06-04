#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

#define CRLF "\r\n"

struct Error *processor_print_nex(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    size_t old_size, line_size;
    char *p;
    
    /* Prefix */
    context->list_index = (style == ES_ENUMERATION) ? (context->list_index + 1) : 0;
    switch (style)
    {
    case ES_INITIALIZE: context->one->size = 0; context->two->size = 0; return OK;
    case ES_FINALIZE: return OK;
    case ES_NORMAL: break;
    case ES_ITEMIZE: PRET(string_append_mem(context->one, STRING_STRLEN(" - "))); break;
    case ES_ENUMERATION: PRET(string_print_append(context->one, " %u. ", context->list_index)); break;
    case ES_QUOTE: PRET(string_append_mem(context->one, STRING_STRLEN(" > "))); break;
    case ES_LARGE:
    case ES_LARGER:
        if (context->one->size > 0) PRET(string_append_mem(context->one, STRING_STRLEN(CRLF)));
        old_size = context->one->size;
        break;
    case ES_LARGEST:
    case ES_HEADER:
        if (context->one->size > 0) PRET(string_append_mem(context->one, STRING_STRLEN(CRLF)));
        old_size = context->one->size; PRET(string_append_mem(context->one, STRING_STRLEN("# ")));
        break;
    case ES_INTERNAL_REFERENCE:
        if (resource[0] == '\0') { PRET(string_print_append(context->one, "=> / ")); }
        else { PRET(string_print_append(context->one, "=> /%s/ ", resource)); }
        break;
    default: /* ES_EXTERNAL_REFERENCE */
        PRET(string_print_append(context->one, "=> %s ", resource));
        break;
    }
    
    /* Text */
    PRET(string_vprint_append(context->one, format, va));

    /* Suffix */
    switch (style)
    {
    case ES_LARGE:
    case ES_LARGER:
        for (p = context->one->p + old_size; p < context->one->p + context->one->size; p++) *p = (*p >= 'a' && *p <= 'z') ? (*p - 'a' + 'A') : *p;
        PRET(string_append_mem(context->one, STRING_STRLEN(CRLF)));
        break;
    case ES_LARGEST:
    case ES_HEADER:
        for (p = context->one->p + old_size; p < context->one->p + context->one->size; p++) *p = (*p >= 'a' && *p <= 'z') ? (*p - 'a' + 'A') : *p;
        PRET(string_append_mem(context->one, STRING_STRLEN(" #" CRLF)));
        line_size = context->one->size - old_size;
        PRET(string_print_append(context->one, "%*s", (int)line_size * 2, ""));
        memcpy(context->one->p + old_size + line_size    , context->one->p + old_size, line_size);
        memcpy(context->one->p + old_size + line_size * 2, context->one->p + old_size, line_size);
        memset(context->one->p + old_size                , '#', line_size - (sizeof(CRLF)-1));
        memset(context->one->p + old_size + line_size * 2, '#', line_size - (sizeof(CRLF)-1));
        break;
    default:
        PRET(string_append_mem(context->one, STRING_STRLEN(CRLF)));
        break;
    }
    return OK;
}

struct ExError processor_process_nex(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(processor_process_finger(request, request_stream, response, response_queue, response_stream));
    return EXOK;
}

struct ExError processor_fixed_nex(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(processor_fixed_finger(fixed, response, response_queue, response_stream));
    return EXOK;
}

void processor_fixed_nex_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream)
{
    processor_fixed_finger_failsafe(fixed, response_stream);
}
