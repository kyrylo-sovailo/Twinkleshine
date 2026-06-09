#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"

struct ExError parser_parse(unsigned char accepting_socket, struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    if (ACCEPTING_SOCKET_IS_HTTP(accepting_socket))                     { EXPRET(parser_parse_http(parser, request, request_stream)); }
    else if (ACCEPTING_SOCKET_IS_HTTPS(accepting_socket))               { EXPRET(parser_parse_http(parser, request, request_stream)); }
    else if (ACCEPTING_SOCKET_IS_GOPHER(accepting_socket))              { EXPRET(parser_parse_gopher(parser, request, request_stream)); }
    else if (ACCEPTING_SOCKET_IS_FINGER(accepting_socket))              { EXPRET(parser_parse_finger(parser, request, request_stream)); }
    else if (ACCEPTING_SOCKET_IS_GEMINI(accepting_socket))              { EXPRET(parser_parse_gemini(parser, request, request_stream)); }
    else if (ACCEPTING_SOCKET_IS_SPARTAN(accepting_socket))             { EXPRET(parser_parse_spartan(parser, request, request_stream)); }
    else if (ACCEPTING_SOCKET_IS_NEX(accepting_socket))                 { EXPRET(parser_parse_nex(parser, request, request_stream)); }
    else /* if (ACCEPTING_SOCKET_IS_GUPPY(client->accepting_socket)) */ { EXPRET(parser_parse_guppy(parser, request, request_stream)); }
    return EXOK;
}
