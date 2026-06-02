#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"
#include "../../include/ring.h"

static struct ExError parser_parse_gopher_part(struct Parser *parser, struct Request *request, const struct ValuePart *part)
{
    const struct ExError EXOK = { OK };
    const char *p;
    for (p = part->p; p < part->p + part->size; p++, request->stream_size++)
    {
        const char c = *p;
        switch (parser->state)
        {
        case RPS_WAIT_RESOURCE_END:
            if (c == '\r') parser->state = RPS_WAIT_FINAL_LINE_LF;
            else if (c == '\n') { parser->tolerated_lf = true; parser->state = RPS_END; }
            else if (c != '\t') request->resource.size++;
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_FINAL_LINE_LF:
            if (c == '\n') parser->state = RPS_END;
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        default: /* case RPS_END: */
            EXRET0("Received symbols after request end", EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            return EXOK; /* Breaking out of switch and for without incrementing offset */
        }
    }
    return EXOK;
}

struct ExError parser_parse_gopher(struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    struct ValueLocation not_parsed_location;
    struct Value not_parsed;
    unsigned char i;
    
    if (parser->state == RPS_BEGIN) parser->state = RPS_WAIT_RESOURCE_END;
    not_parsed_location.offset = request->stream_size;
    if (request_stream->size >= MAX_REQUEST_SIZE) not_parsed_location.size = MAX_REQUEST_SIZE - not_parsed_location.offset;
    else not_parsed_location.size = request_stream->size - not_parsed_location.offset;
    EXPRETF(ring_get(request_stream, &not_parsed_location, false, &not_parsed), EEF_CLOSE_LOG_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRET(parser_parse_gopher_part(parser, request, &not_parsed.parts[i]));
    }
    if (request_stream->size >= MAX_REQUEST_SIZE)
    {
        EXARET0(parser->state == RPS_END, "Actual request size exceeded", EEF_SEND_SHUTDOWN_LOG(FR_MAX_REQUEST_HEADER_SIZE));
    }
    return EXOK;
}