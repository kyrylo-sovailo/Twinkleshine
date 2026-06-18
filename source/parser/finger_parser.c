#include "../../include/parser.h"
#include "../../commonlib/include/error.h"

struct ExError parser_parse_finger(struct Parser *parser, struct Request *request, const struct ConstantContinuousValue *request_stream)
{
    const struct ExError EXOK = { OK };
    const char *p;
    if (parser->state == RPS_BEGIN) parser->state = RPS_WAIT_METHOD_BEGIN;
    for (p = request_stream->p; p < request_stream->p + request_stream->size; p++, request->stream_size++)
    {
        const char c = *p;
        switch (parser->state)
        {
        case RPS_WAIT_METHOD_BEGIN:
            if (c == '\r') parser->state = RPS_WAIT_FINAL_LINE_LF;
            else if (c == '\n') { parser->tolerated_lf = true; parser->state = RPS_END; }
            else if (c == '/') { request->method.size = 1; parser->state = RPS_WAIT_METHOD_END; }
            else { request->resource.size = 1; parser->state = RPS_WAIT_RESOURCE_END; }
            break;
            
        case RPS_WAIT_METHOD_END:
            if (c == 'W') { request->method.size = 2; parser->state = RPS_WAIT_RESOURCE_BEGIN; }
            else EXRET1("Invalid symbol: %d", (int)c, EEF_SEND_SHUTDOWN_LOG(FR_REQUEST_INVALID));
            break;

        case RPS_WAIT_RESOURCE_BEGIN:
            if (c == '\r') parser->state = RPS_WAIT_FINAL_LINE_LF;
            else if (c == '\n') { parser->tolerated_lf = true; parser->state = RPS_END; }
            else if (c == ' ') { /* Do nothing */ }
            else { request->method.offset = request->stream_size; request->resource.size = 1; parser->state = RPS_WAIT_RESOURCE_END; }
            break;

        case RPS_WAIT_RESOURCE_END:
            if (c == '\r') parser->state = RPS_WAIT_FINAL_LINE_LF;
            else if (c == '\n') { parser->tolerated_lf = true; parser->state = RPS_END; }
            else { request->resource.size++; }
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
