#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/tables.h"

static struct ExError parser_parse_gemini_guppy(struct Parser *parser, struct Request *request, const struct ConstantContinuousValue *request_stream, bool mode_guppy)
{
    const struct ExError EXOK = { OK };
    const char *p;
    const char gemini[] = "gemini://";
    const char guppy[] = "guppy://";
    const char *gemini_guppy = mode_guppy ? guppy : gemini;
    const unsigned char gemini_size = sizeof(gemini) - 1;
    const unsigned char guppy_size = sizeof(guppy) - 1;
    const unsigned char gemini_guppy_size = mode_guppy ? guppy_size : gemini_size;
    if (parser->state == RPS_BEGIN) parser->state = RPS_WAIT_PROTOCOL_END;
    for (p = request_stream->p; p < request_stream->p + request_stream->size; p++, request->stream_size++)
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

struct ExError parser_parse_gemini(struct Parser *parser, struct Request *request, const struct ConstantContinuousValue *request_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(parser_parse_gemini_guppy(parser, request, request_stream, false));
    return EXOK;
}

struct ExError parser_parse_guppy(struct Parser *parser, struct Request *request, const struct ConstantContinuousValue *request_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(parser_parse_gemini_guppy(parser, request, request_stream, true));
    return EXOK;
}
