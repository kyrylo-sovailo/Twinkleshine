#include "../../include/parser.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"
#include "../../include/ring.h"

struct ExError parser_parse(unsigned char accepting_socket, struct Parser *parser, struct Request *request, const struct Ring *request_stream)
{
    const struct ExError EXOK = { OK };
    struct ValueLocation not_parsed_location;
    union Value not_parsed;
    unsigned char i;
    
    not_parsed_location.offset = request->stream_size;
    if (request_stream->size >= MAX_REQUEST_SIZE) not_parsed_location.size = MAX_REQUEST_SIZE - not_parsed_location.offset;
    else not_parsed_location.size = request_stream->size - not_parsed_location.offset;
    EXPRETF(ring_get(request_stream, &not_parsed_location, false, &not_parsed.m), EEF_CLOSE_LOG_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        if (ACCEPTING_SOCKET_IS_HTTP(accepting_socket))                     { EXPRET(parser_parse_http(parser, request, request_stream, &not_parsed.c.parts[i])); }
        else if (ACCEPTING_SOCKET_IS_HTTPS(accepting_socket))               { EXPRET(parser_parse_http(parser, request, request_stream, &not_parsed.c.parts[i])); }
        else if (ACCEPTING_SOCKET_IS_GOPHER(accepting_socket))              { EXPRET(parser_parse_gopher(parser, request, &not_parsed.c.parts[i])); }
        else if (ACCEPTING_SOCKET_IS_FINGER(accepting_socket))              { EXPRET(parser_parse_finger(parser, request, &not_parsed.c.parts[i])); }
        else if (ACCEPTING_SOCKET_IS_GEMINI(accepting_socket))              { EXPRET(parser_parse_gemini(parser, request, &not_parsed.c.parts[i])); }
        else if (ACCEPTING_SOCKET_IS_SPARTAN(accepting_socket))             { EXPRET(parser_parse_spartan(parser, request, request_stream, &not_parsed.c.parts[i])); }
        else if (ACCEPTING_SOCKET_IS_NEX(accepting_socket))                 { EXPRET(parser_parse_nex(parser, request, &not_parsed.c.parts[i])); }
        else if (ACCEPTING_SOCKET_IS_TEXT(accepting_socket))                { EXPRET(parser_parse_text(parser, request, &not_parsed.c.parts[i])); }
        else /* if (ACCEPTING_SOCKET_IS_GUPPY(client->accepting_socket)) */ { EXPRET(parser_parse_guppy(parser, request, &not_parsed.c.parts[i])); }
    }
    if (request_stream->size >= MAX_REQUEST_SIZE)
    {
        EXARET0(parser->state == RPS_END, "Actual request size exceeded", EEF_SEND_SHUTDOWN_LOG(FR_MAX_REQUEST_HEADER_SIZE));
    }
    return EXOK;
}
