#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"
#include "../../include/language.h"
#include "../../include/macro.h"
#include "../../include/ring.h"
#include "../../include/tables.h"

#include <stdlib.h>
#include <string.h>

enum LanguageParserState
{
    LPS_WAIT_LANGUAGE_BEGIN,
    LPS_WAIT_LANGUAGE_FIRST_CHARACTERS,
    LPS_WAIT_COMMA_SEMICOLON,
    LPS_WAIT_Q,
    LPS_WAIT_EQUAL,
    LPS_WAIT_NUMBER
};

static void parser_parse_http_language_commit(const char language[MAX_LANGUAGE_SIZE], unsigned char language_size, double q, char *best_language, double *best_q)
{
    char result;
    if (q != q) q = 0.0;
    else if (q > 1.0) q = 1.0;
    else if (q < 0.0) q = 0.0;
    result = language_get(language, language_size);
    if (result != '\0' && q > *best_q)
    {
        *best_language = result;
        *best_q = q;
    }
}

static char parser_parse_http_language(const struct ConstantValue *accept_language)
{
    enum LanguageParserState state = LPS_WAIT_LANGUAGE_BEGIN;
    char best_language = '\0';
    double best_q = 0.0;
    
    char language[MAX_LANGUAGE_SIZE];
    unsigned char language_size;
    double q; double q_multiplier;
    
    unsigned char i;

    for (i = 0; i < VALUE_PARTS; i++)
    {
        const char *p;
        for (p = accept_language->parts[i].p; p < accept_language->parts[i].p + accept_language->parts[i].size; p++)
        {
            const char c = *p;
            switch (state)
            {
            case LPS_WAIT_LANGUAGE_BEGIN:
                if (c == ' ')
                {
                    /* Do nothing */
                }
                else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '*')
                {
                    language[0] = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
                    language[1] = '\0';
                    language_size = 1;
                    q = 0.0;
                    q_multiplier = -1.0;
                    state = LPS_WAIT_LANGUAGE_FIRST_CHARACTERS;
                }
                else return best_language;
                break;

            case LPS_WAIT_LANGUAGE_FIRST_CHARACTERS:
                if (c == ' ' || c == '-')
                {
                    state = LPS_WAIT_COMMA_SEMICOLON;
                }
                else if (c == ',')
                {
                    parser_parse_http_language_commit(language, language_size, 1.0, &best_language, &best_q);
                    state = LPS_WAIT_LANGUAGE_BEGIN;
                }
                else if (c == ';')
                {
                    state = LPS_WAIT_Q;
                }
                else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '*')
                {
                    language[language_size] = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
                    language_size++;
                    if (language_size == MAX_LANGUAGE_SIZE) state = LPS_WAIT_COMMA_SEMICOLON;
                }
                else return best_language;
                break;

            case LPS_WAIT_COMMA_SEMICOLON:
                if (c == ' ' || c == '-')
                {
                    /* Do nothing */
                }
                else if (c == ',')
                {
                    parser_parse_http_language_commit(language, language_size, 1.0, &best_language, &best_q);
                    state = LPS_WAIT_LANGUAGE_BEGIN;
                }
                else if (c == ';')
                {
                    state = LPS_WAIT_Q;
                }
                else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '*')
                {
                    language_size = MAX_LANGUAGE_SIZE + 1;
                }
                else return best_language;
                break;

            case LPS_WAIT_Q:
                if (c == ' ')
                {
                    /* Do nothing */
                }
                else if (c == 'Q' || c == 'q')
                {
                    state = LPS_WAIT_EQUAL;
                }
                else return best_language;
                break;

            case LPS_WAIT_EQUAL:
                if (c == ' ')
                {
                    /* Do nothing */
                }
                else if (c == '=')
                {
                    state = LPS_WAIT_NUMBER;
                }
                else return best_language;
                break;

            case LPS_WAIT_NUMBER:
                if (c == ' ')
                {
                    /* Do nothing */
                }
                else if (c == ',')
                {
                    parser_parse_http_language_commit(language, language_size, q, &best_language, &best_q);
                    state = LPS_WAIT_LANGUAGE_BEGIN;
                }
                else if (c == '.')
                {
                    q_multiplier = 1.0;
                }
                else if (c >= '0' && c <= '9')
                {
                    if (q_multiplier < 0.0)  q = 10.0 * q + (c - '0');
                    else { q_multiplier /= 10.0; q += q_multiplier * (c - '0'); }
                }
                else return best_language;
                break;
            }
        }
    }

    if (state == LPS_WAIT_NUMBER)
    {
        parser_parse_http_language_commit(language, language_size, q, &best_language, &best_q);
    }

    return best_language;
}

static struct ExError parser_parse_http_name_value(struct Parser *parser, struct Request *request, const struct Ring *request_stream) NODISCARD;
static struct ExError parser_parse_http_name_value(struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    union Value name;
    EXPRETF(ring_get(request_stream, &parser->current_name, false, &name.m), EEF_CLOSE_LOG_DIE);
    if (value_compare_case_mem(&name.c, STRING_STRLEN("content-length")))
    {
        union Value value;
        EXPRETF(ring_get(request_stream, &parser->current_value, false, &value.m), EEF_CLOSE_LOG_DIE);
        value_trim(&value.c);
        EXARET0(value_to_uint(&value.c, &parser->remaining_content),
            "Invalid integer",
            EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
        EXARET0(parser->current_value.offset + parser->current_value.size + parser->remaining_content <= MAX_REQUEST_SIZE,
            "Promised request size exceeded",
            EEF_SEND_SHUTDOWN_LOG(FR_MAX_REQUEST_CONTENT_SIZE));
    }
    else if (value_compare_case_mem(&name.c, STRING_STRLEN("connection")))
    {
        union Value value;
        EXPRETF(ring_get(request_stream, &parser->current_value, false, &value.m), EEF_CLOSE_LOG_DIE);
        while (true)
        {
            union Value current_value;
            const bool parts_remaining = value_parse_comma(&value.c, &current_value.c);
            value_trim(&current_value.c);
            if (value_compare_case_mem(&current_value.c, STRING_STRLEN("keep-alive"))) { request->keep_alive = true; break; }
            if (!parts_remaining) break;
        }
    }
    else if (value_compare_case_mem(&name.c, STRING_STRLEN("user-agent")))
    {
        request->user = parser->current_value;
    }
    else if (value_compare_case_mem(&name.c, STRING_STRLEN("accept-language")))
    {
        union Value value;
        EXPRETF(ring_get(request_stream, &parser->current_value, false, &value.m), EEF_CLOSE_LOG_DIE);
        request->language = parser_parse_http_language(&value.c);
    }
    return EXOK;
}

struct ExError parser_parse_http(struct Parser *parser, struct Request *request, const struct Ring *request_stream, const struct ConstantContinuousValue *part)
{
    const struct ExError EXOK = { OK };
    const char *p;
    if (parser->state == RPS_BEGIN) parser->state = RPS_WAIT_METHOD_BEGIN;
    for (p = part->p; p < part->p + part->size; p++, request->stream_size++)
    {
        const char c = *p;
        const unsigned char c_type = character_map[(unsigned char)c];
        size_t available_content;
        switch (parser->state)
        {
        case RPS_WAIT_METHOD_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_METHOD) != 0)
            {
                request->method.offset = request->stream_size;
                request->method.size = 1;
                parser->state = RPS_WAIT_METHOD_END;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;
            
        case RPS_WAIT_METHOD_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_RESOURCE_BEGIN;
            }
            else if ((c_type & CM_METHOD) != 0)
            {
                request->method.size++;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_RESOURCE_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_RESOURCE) != 0)
            {
                request->resource.offset = request->stream_size;
                request->resource.size = 1;
                parser->state = RPS_WAIT_RESOURCE_END;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_RESOURCE_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_PROTOCOL_BEGIN;
            }
            else if ((c_type & CM_RESOURCE) != 0)
            {
                request->resource.size++;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_PROTOCOL_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_PROTOCOL) != 0)
            {
                request->protocol.offset = request->stream_size;
                request->protocol.size = 1;
                parser->state = RPS_WAIT_PROTOCOL_END;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_PROTOCOL_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_LINE_CR;
            }
            else if ((c_type & CM_PROTOCOL) != 0)
            {
                request->protocol.size++;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r') parser->state = RPS_WAIT_LINE_LF;
                else { parser->tolerated_lf = true; parser->state = RPS_WAIT_NAME_BEGIN; } /* Tolerating LF ending */
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_LINE_CR:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r') parser->state = RPS_WAIT_LINE_LF;
                else { parser->tolerated_lf = true; parser->state = RPS_WAIT_NAME_BEGIN; } /* Tolerating LF ending */
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_LINE_LF:
            if ((c_type & CM_SPACE) != 0) /* Tolerating CR ending */
            {
                parser->tolerated_cr = true;
                parser->state = RPS_WAIT_NAME_BEGIN;
            }
            else if ((c_type & CM_NAME) != 0) /* Tolerating CR ending */
            {
                parser->tolerated_cr = true;
                parser->current_name.offset = request->stream_size;
                parser->current_name.size = 1;
                parser->state = RPS_WAIT_NAME_END;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r')
                {
                    parser->tolerated_cr = true;
                    parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received CR CR */
                }
                else
                {
                    parser->state = RPS_WAIT_NAME_BEGIN;
                }
            }
            break;

        case RPS_WAIT_NAME_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_NAME) != 0)
            {
                parser->current_name.offset = request->stream_size;
                parser->current_name.size = 1;
                parser->state = RPS_WAIT_NAME_END;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r')
                {
                    if (!parser->tolerated_cr) parser->state = RPS_WAIT_FINAL_LINE_LF; /* Received CR LF CR */
                    else parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received CR CR */
                }
                else
                {
                    parser->tolerated_lf = true;
                    parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received ?? LF */
                }
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;
            
        case RPS_WAIT_NAME_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_COLON;
            }
            else if ((c_type & CM_NAME) != 0)
            {
                parser->current_name.size++;
            }
            else if (c == ':')
            {
                parser->current_value.offset = request->stream_size + 1;
                parser->current_value.size = 0;
                parser->state = RPS_WAIT_VALUE_END;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_COLON:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if (c == ':')
            {
                parser->current_value.offset = request->stream_size + 1;
                parser->current_value.size = 0;
                parser->state = RPS_WAIT_VALUE_END;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;
            
        case RPS_WAIT_VALUE_END:
            if ((c_type & CM_VALUE) != 0)
            {
                parser->current_value.size++;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                EXPRET(parser_parse_http_name_value(parser, request, request_stream));
                if (c == '\r')
                {
                    if (!parser->tolerated_cr) parser->state = RPS_WAIT_LINE_LF;
                    else parser->state = RPS_WAIT_NAME_BEGIN;
                }
                else
                {
                    parser->tolerated_lf = true;
                    parser->state = RPS_WAIT_NAME_BEGIN;
                }
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_FINAL_LINE_LF:
            if (c == '\n')
            {
                parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received CR LF CR LF */
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_CONTENT:
            /* Received the first symbol of centent, processing content in bulk */
            available_content = part->size - (size_t)(p - part->p);
            if (available_content >= parser->remaining_content)
            {
                p += parser->remaining_content;
                request->stream_size += parser->remaining_content;
                parser->state = RPS_END;
            }
            else
            {
                p += available_content; /* aka p = part->p + part->size */
                request->stream_size += available_content;
            }
            return EXOK; /* Breaking out of switch and for without incrementing offset */

        default: /* case RPS_END: */
            EXARET0(request->keep_alive, "Received symbols after request end", EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            return EXOK; /* Breaking out of switch and for without incrementing offset */
        }
    }
    return EXOK;
}
