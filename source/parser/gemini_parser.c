#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/macro.h"
#include "../../include/tables.h"

static struct ExError parser_parse_gemini_like(struct Parser *parser, struct Request *request, const struct ConstantContinuousValue *request_stream, const char *protocol, unsigned char protocol_size)
{
    const struct ExError EXOK = { OK };
    const char *p;
    if (parser->state == RPS_BEGIN) parser->state = RPS_WAIT_PROTOCOL_END;
    for (p = request_stream->p; p < request_stream->p + request_stream->size; p++, request->stream_size++)
    {
        const char c = *p;
        const unsigned char c_type = character_map[(unsigned char)c];
        switch (parser->state)
        {
        case RPS_WAIT_PROTOCOL_END:
            if (c == protocol[request->protocol.size])
            {
                request->protocol.size++;
                if (request->protocol.size == protocol_size) { request->method.offset = protocol_size; parser->state = RPS_WAIT_METHOD_END; }
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
    EXPRET(parser_parse_gemini_like(parser, request, request_stream, STRING_STRLEN("gemini://")));
    return EXOK;
}

struct ExError parser_parse_guppy(struct Parser *parser, struct Request *request, const struct ConstantContinuousValue *request_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(parser_parse_gemini_like(parser, request, request_stream, STRING_STRLEN("guppy://")));
    return EXOK;
}

struct ExError parser_parse_text(struct Parser *parser, struct Request *request, const struct ConstantContinuousValue *request_stream)
{
    const struct ExError EXOK = { OK };
    EXPRET(parser_parse_gemini_like(parser, request, request_stream, STRING_STRLEN("text://")));
    return EXOK;
}
