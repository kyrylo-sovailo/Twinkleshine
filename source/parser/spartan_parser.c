#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"
#include "../../include/ring.h"
#include "../../include/tables.h"

static struct ExError parser_parse_spartan_part(struct Parser *parser, struct Request *request, const struct Ring *request_stream, const struct ValuePart *part)
{
    const struct ExError EXOK = { OK };
    const char *p;
    size_t available_content;

    for (p = part->p; p < part->p + part->size; p++, request->stream_size++)
    {
        const char c = *p;
        const unsigned char c_type = character_map[(unsigned char)c];
        switch (parser->state)
        {
        case RPS_WAIT_METHOD_END:
            if (c == ' ')
            {
                parser->state = RPS_WAIT_RESOURCE_BEGIN;
            }
            else if ((c_type & CM_DOMAIN) != 0)
            {
                request->method.size++;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_RESOURCE_BEGIN:
            if (c == ' ')
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
            if (c == ' ')
            {
                parser->state = RPS_WAIT_NAME_BEGIN;
            }
            else if ((c_type & CM_RESOURCE) != 0)
            {
                request->resource.size++;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_NAME_BEGIN: /* Repurposed for content length */
            if (c == ' ')
            {
                /* Do nothing */
            }
            else if ((c_type & CM_RESOURCE) != 0) /* TODO: Not enough bits in character table, check if valid number later */
            {
                parser->current_value.offset = request->stream_size;
                parser->current_value.size = 1;
                parser->state = RPS_WAIT_NAME_END;
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;
            
        case RPS_WAIT_NAME_END:
            if ((c_type & CM_RESOURCE) != 0)
            {
                parser->current_value.size++;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                struct Value value;
                EXPRETF(ring_get(request_stream, &parser->current_value, false, &value),
                    EEF_CLOSE_LOG_DIE);
                EXARET0(value_to_uint(&value, &parser->remaining_content),
                    "Invalid integer",
                    EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
                EXARET0(parser->current_value.offset + parser->current_value.size + parser->remaining_content <= MAX_REQUEST_SIZE,
                    "Promised request size exceeded",
                    EEF_SEND_SHUTDOWN_LOG(FR_MAX_REQUEST_CONTENT_SIZE));
                if (c == '\r') parser->state = RPS_WAIT_FINAL_LINE_LF;
                else if (c == '\n') { parser->tolerated_lf = true; parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; }
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_FINAL_LINE_LF:
            if (c == '\n') parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END;
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
            EXRET0("Received symbols after request end", EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            return EXOK; /* Breaking out of switch and for without incrementing offset */
        }
    }
    return EXOK;
}

struct ExError parser_parse_spartan(struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    struct ValueLocation not_parsed_location;
    struct Value not_parsed;
    unsigned char i;
    
    if (parser->state == RPS_BEGIN) parser->state = RPS_WAIT_METHOD_END;
    not_parsed_location.offset = request->stream_size;
    if (request_stream->size >= MAX_REQUEST_SIZE) not_parsed_location.size = MAX_REQUEST_SIZE - not_parsed_location.offset;
    else not_parsed_location.size = request_stream->size - not_parsed_location.offset;
    EXPRETF(ring_get(request_stream, &not_parsed_location, false, &not_parsed), EEF_CLOSE_LOG_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRET(parser_parse_spartan_part(parser, request, request_stream, &not_parsed.parts[i]));
    }
    if (request_stream->size >= MAX_REQUEST_SIZE)
    {
        EXARET0(parser->state == RPS_END, "Actual request size exceeded", EEF_SEND_SHUTDOWN_LOG(FR_MAX_REQUEST_HEADER_SIZE));
    }
    return EXOK;
}
