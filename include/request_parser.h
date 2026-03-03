#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H

#include "error.h"
#include "request_response.h"

/* Client's state machine */
enum RequestParserState
{
    RPS_WAIT_METHOD_BEGIN = 0,
    RPS_WAIT_METHOD_END,
    RPS_WAIT_RESOURCE_BEGIN,
    RPS_WAIT_RESOURCE_END,
    RPS_WAIT_PROTOCOL_BEGIN,
    RPS_WAIT_PROTOCOL_END,
    RPS_WAIT_LINE_CR,
    RPS_WAIT_LINE_LF,

    RPS_WAIT_NAME_BEGIN,
    RPS_WAIT_NAME_END,
    RPS_WAIT_COLON,
    RPS_WAIT_VALUE_END,

    RPS_WAIT_FINAL_LINE_LF,
    RPS_WAIT_CONTENT,
    RPS_END
};

/* Client's state */
struct RequestParser
{
    enum RequestParserState state;  /* Current state */
    unsigned int remaining_content; /* Expected content length */
    bool tolerated_lf, tolerated_cr;/* Flags that indicate non-conforming implementations, used to avoid waiting for line ends that aren't coming */
    struct ValueRange current_name; /* Field name */
    struct ValueRange current_value;/* Field value */
    struct Request request;         /* Current output */
};

/* Feed buffer to parser */
struct Error *parse_request(struct RequestParser *parser, const struct CharBuffer *buffer, size_t *buffer_used) NODISCARD;

/* Prepare the parser for the first parse */
void parse_request_initialize(struct RequestParser *parser);

/* Prepare the parser for the next parse */
void parse_request_reset(struct RequestParser *parser);

/* Finalize the parser */
void parse_request_finalize(struct RequestParser *parser);

#endif
