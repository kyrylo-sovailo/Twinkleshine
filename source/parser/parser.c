#include "../../include/parser.h"
#include "../../include/client.h"

struct ExError parser_parse(int type, struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    switch (type)
    {
    case CT_HTTP:   EXPRET(parser_parse_http(parser, request, request_stream));     break;
    case CT_GOPHER: EXPRET(parser_parse_gopher(parser, request, request_stream));   break;
    case CT_FINGER: EXPRET(parser_parse_finger(parser, request, request_stream));   break;
    case CT_GEMINI: EXPRET(parser_parse_gemini(parser, request, request_stream));   break;
    case CT_SPARTAN:EXPRET(parser_parse_spartan(parser, request, request_stream));  break;
    default:        EXPRET(parser_parse_gopher(parser, request, request_stream));   break; /* CT_NEX */
    }
    return EXOK;
}
