#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"
#include "../../include/ring.h"
#include "../../include/tables.h"

static struct ExError parser_parse_gemini_guppy_part(struct Parser *parser, struct Request *request, const struct ValuePart *part, bool mode_guppy)
{
    const struct ExError EXOK = { OK };
    const char *p;
    const char gemini[] = "gemini://";
    const char guppy[] = "guppy://";
    const char *gemini_guppy = mode_guppy ? guppy : gemini;
    const size_t gemini_guppy_size = mode_guppy ? (sizeof(guppy) - 1) : (sizeof(gemini) - 1);
    for (p = part->p; p < part->p + part->size; p++, request->stream_size++)
    {
        const char c = *p;
        const unsigned char c_type = character_map[(unsigned char)c];
        switch (parser->state)
        {
        case RPS_WAIT_PROTOCOL_END:
            if (c == gemini_guppy[request->protocol.size])
            {
                request->protocol.size++;
                if (request->protocol.size == gemini_guppy_size) { request->method.offset = gemini_guppy_size; parser->state = RPS_WAIT_METHOD_END; }
            }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_METHOD_END:
            if ((c_type & CM_DOMAIN) != 0 || c == ':') { request->method.size++; } /* Colon because Gemini can also receive port number */
            else if (c == '/') { request->resource.offset = request->stream_size; request->resource.size = 1; parser->state = RPS_WAIT_RESOURCE_END; }
            else if (c == '\r') parser->state = RPS_WAIT_FINAL_LINE_LF;
            else if (c == '\n') { parser->tolerated_lf = true; parser->state = RPS_END; }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_RESOURCE_END:
            if ((c_type & CM_RESOURCE) != 0) { request->resource.size++; } /* Why did the spec mention BOM? BOM is automatically not a valid STD66 */
            else if (c == '\r') parser->state = RPS_WAIT_FINAL_LINE_LF;
            else if (c == '\n') { parser->tolerated_lf = true; parser->state = RPS_END; }
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

static struct ExError parser_parse_gemini_guppy(struct Parser *parser, struct Request *request, const struct Ring *request_stream, bool mode_guppy)
{
    const struct ExError EXOK = { OK };
    struct ValueLocation not_parsed_location;
    struct Value not_parsed;
    unsigned char i;
    
    if (parser->state == RPS_BEGIN) parser->state = RPS_WAIT_PROTOCOL_END;
    not_parsed_location.offset = request->stream_size;
    if (request_stream->size >= MAX_REQUEST_SIZE) not_parsed_location.size = MAX_REQUEST_SIZE - not_parsed_location.offset;
    else not_parsed_location.size = request_stream->size - not_parsed_location.offset;
    EXPRETF(ring_get(request_stream, &not_parsed_location, false, &not_parsed), EEF_CLOSE_LOG_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRET(parser_parse_gemini_guppy_part(parser, request, &not_parsed.parts[i], mode_guppy));
    }
    if (request_stream->size >= MAX_REQUEST_SIZE)
    {
        EXARET0(parser->state == RPS_END, "Actual request size exceeded", EEF_SEND_SHUTDOWN_LOG(FR_MAX_REQUEST_HEADER_SIZE));
    }
    return EXOK;
}

struct ExError parser_parse_gemini(struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(parser_parse_gemini_guppy(parser, request, request_stream, false));
    return EXOK;
}

struct ExError parser_parse_guppy(struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(parser_parse_gemini_guppy(parser, request, request_stream, true));
    return EXOK;
}
