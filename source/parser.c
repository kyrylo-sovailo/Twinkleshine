#include "../include/parser.h"
#include "../commonlib/include/error.h"
#include "../include/constants.h"
#include "../include/error.h"
#include "../include/processor.h"
#include "../include/ring.h"
#include "../include/tables.h"

#include <stdlib.h>
#include <string.h>

static struct ExError parser_parse_name_value(struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    struct Value name;
    EXPRETF(ring_get(request_stream, &parser->current_name, false, &name), EEF_CLOSE_LOG_DIE);
    if (value_compare_case_mem(&name, "content-length", strlen("content-length")))
    {
        struct Value value;
        EXPRETF(ring_get(request_stream, &parser->current_value, false, &value), EEF_CLOSE_LOG_DIE);
        value_trim(&value);
        EXARET0(value_to_uint(&value, &parser->remaining_content),
            "Invalid integer",
            EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
        EXARET0(parser->current_value.offset + parser->current_value.size + parser->remaining_content <= MAX_REQUEST_SIZE,
            "Promised request size exceeded",
            EEF_SEND_CLOSE_LOG(FR_MAX_REQUEST_CONTENT_SIZE));
    }
    else if (value_compare_case_mem(&name, "connection", strlen("connection")))
    {
        struct Value value;
        EXPRETF(ring_get(request_stream, &parser->current_value, false, &value), EEF_CLOSE_LOG_DIE);
        while (true)
        {
            struct Value current_value;
            const bool parts_remaining = value_parse_comma(&value, &current_value);
            value_trim(&current_value);
            if (value_compare_case_mem(&current_value, "keep-alive", strlen("keep-alive"))) { request->keep_alive = true; break; }
            if (!parts_remaining) break;
        }
    }
    return EXOK;
}

static struct ExError parser_parse_part(struct Parser *parser, struct Request *request, const struct Ring *request_stream, const struct ValuePart *part)
{
    const struct ExError EXOK = { OK };
    const char *p;
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
            break;
            
        case RPS_WAIT_VALUE_END:
            if ((c_type & CM_VALUE) != 0)
            {
                parser->current_value.size++;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                EXPRET(parser_parse_name_value(parser, request, request_stream));
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
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_FINAL_LINE_LF:
            if (c == '\n')
            {
                parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received CR LF CR LF */
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_CLOSE_LOG(FR_REQUEST_INVALID));
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

        default: /* RPS_END */
            return EXOK; /* Breaking out of switch and for without incrementing offset */
        }
    }
    return EXOK;
}

struct ExError parser_parse(struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    struct ValueLocation not_parsed_location;
    struct Value not_parsed;
    unsigned char i;
    
    not_parsed_location.offset = request->stream_size;
    if (request_stream->size >= MAX_REQUEST_SIZE) not_parsed_location.size = MAX_REQUEST_SIZE - not_parsed_location.offset;
    else not_parsed_location.size = request_stream->size - not_parsed_location.offset;
    EXPRETF(ring_get(request_stream, &not_parsed_location, false, &not_parsed), EEF_CLOSE_LOG_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRET(parser_parse_part(parser, request, request_stream, &not_parsed.parts[i]));
    }
    if (request_stream->size >= MAX_REQUEST_SIZE)
    {
        EXARET0(parser->state == RPS_END, "Actual request size exceeded", EEF_SEND_CLOSE_LOG(FR_MAX_REQUEST_HEADER_SIZE));
    }
    return EXOK;
}