#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/language.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

#define CRLF "\r\n"
#define ENDLINE "\n" /* Specification doesn't say anything, but LF is extremely convenient for lien wrap */

static void wrap(char *text, size_t size)
{
    const size_t line_width = 70;
    size_t symbols_till_want_newline = line_width;
    char *last_space = NULL;
    char *p;
    for (p = text; p < text + size; p++)
    {
        const char c = *p;
        if (c == ' ') last_space = p;
        if (c == '\n') { symbols_till_want_newline = line_width; last_space = NULL; continue; } /* Forced break line */
        if (symbols_till_want_newline > 0) { symbols_till_want_newline--; continue; } /* Don't want to break line */
        if (last_space != NULL) { symbols_till_want_newline = line_width - (size_t)(p - last_space); *last_space = '\n'; last_space = NULL; } /* Want and can break line */
        /* Want break line but can't */
    }
}

struct ExError processor_process_nex(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(processor_process_finger(request, request_stream, response, response_queue, response_stream));
    return EXOK;
}

struct ExError processor_fixed_nex(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(processor_fixed_finger(fixed, response, response_queue, response_stream));
    return EXOK;
}

void processor_fixed_nex_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream)
{
    processor_fixed_finger_failsafe(fixed, response_stream);
}


struct Error *processor_print_nex(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    /* Pre-prefix */
    const char *language;
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
    case ES_NORMAL:
    case ES_PARAGRAPH: break;
    case ES_ITEMIZE: PRET(string_append_mem(context->one, STRING_STRLEN(" - "))); break;
    case ES_ENUMERATION: PRET(string_print_append(context->one, " %u. ", context->list_index)); break;
    case ES_QUOTE: PRET(string_append_mem(context->one, STRING_STRLEN(" > "))); break;
    case ES_LARGE:
    case ES_LARGER: break;
    case ES_LARGEST:
    case ES_HEADER: PRET(string_append_mem(context->one, STRING_STRLEN("# "))); break;
    case ES_INTERNAL_REFERENCE:
        language = language_slash(context->language, context->requested_language, context->request->language);
        if (resource[0] == '\0') { PRET(string_print_append(context->one, "=> %s/ ", language)); }
        else { PRET(string_print_append(context->one, "=> /%s%s/ ", resource, language)); }
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
    case ES_NORMAL:
    case ES_PARAGRAPH:
        wrap(context->one->p + old_size, context->one->size - old_size);
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        break;
    case ES_LARGE:
    case ES_LARGER:
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        processor_generic_upper_case(context->one, old_size, 0, sizeof(ENDLINE)-1);
        break;
    case ES_LARGEST:
    case ES_HEADER:
        PRET(string_append_mem(context->one, STRING_STRLEN(" #" ENDLINE)));
        processor_generic_upper_case(context->one, old_size, 0, sizeof(ENDLINE)-1);
        PRET(processor_generic_box(context->one, old_size, 0, sizeof(ENDLINE)-1, '#', '#'));
        break;
    default:
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        break;
    }
    return OK;
}
