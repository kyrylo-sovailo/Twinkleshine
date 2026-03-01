#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H

#include "error.h"
#include "request_response.h"
#include "ring.h"

/* Client's state machine */
enum RequestParserState
{
    RPS_WAIT_METHOD_BEGIN,
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
    struct Value current_name;      /* Field name */
    struct Value current_value;     /* Field value */
    struct Request request;         /* Current output */
};

/* Feed buffer to parser */
struct Error *parse_request(struct RequestParser *parser, struct Ring *ring) NODISCARD;

#endif
