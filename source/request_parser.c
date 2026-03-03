#include "../include/request_parser.h"

#include <stdlib.h>
#include <string.h>

enum CharacterMap
{
    CM_NONE = 0,
    CM_SPACE = 1,
    CM_NEWLINE = 2,
    CM_METHOD = 4,
    CM_RESOURCE = 8,
    CM_PROTOCOL = 16,
    CM_NAME = 32,
    CM_VALUE = 64
};

static unsigned char character_map[256];
static bool initialized = false;

static void initialize(void)
{
    const char *p;
    char c;
    memset(character_map, 0, sizeof(character_map));

    /* Space */
    character_map[' '] |= CM_SPACE;
    character_map['\t'] |= CM_SPACE;
    character_map['\v'] |= CM_SPACE;

    /* Newline */
    character_map['\n'] |= CM_NEWLINE;
    character_map['\r'] |= CM_NEWLINE;

    /* Allowed in method */
    for (c = 'A'; c <= 'Z'; c++) character_map[(unsigned char)c] |= CM_METHOD;
    for (c = 'a'; c <= 'z'; c++) character_map[(unsigned char)c] |= CM_METHOD;
    for (c = '0'; c <= '9'; c++) character_map[(unsigned char)c] |= CM_METHOD;
    for (p = "!#$%&'*+-.^_`|~"; *p != '\0'; p++) character_map[(unsigned char)*p] |= CM_METHOD;

    /* Allowed in resource */
    for (c = '!'; c <= '~'; c++) character_map[(unsigned char)c] |= CM_RESOURCE; /*All visible ASCII*/

    /* Allowed in protocol */
    for (p = "HTTP/1.0"; *p != '\0'; p++) character_map[(unsigned char)*p] |= CM_PROTOCOL;

    /* Allowed in name */
    for (c = 'A'; c <= 'Z'; c++) character_map[(unsigned char)c] |= CM_NAME;
    for (c = 'a'; c <= 'z'; c++) character_map[(unsigned char)c] |= CM_NAME;
    for (c = '0'; c <= '9'; c++) character_map[(unsigned char)c] |= CM_NAME;
    for (p = "!#$%&'*+-.^_`|~"; *p != '\0'; p++) character_map[(unsigned char)*p] |= CM_NAME;

    /* Allowed in value */
    for (c = ' '; c <= '~'; c++) character_map[(unsigned char)c] |= CM_VALUE; /*All visible ASCII and space*/
}

static struct Value parse_construct_value(struct RequestParser *parser, const struct CharBuffer *buffer, size_t initial_buffer_used, const struct ValueRange *range)
{
    const size_t initial_offset = parser->request.data.size;
    struct Value value = { 0 };
    
    if (initial_offset > range->offset) value.size[0] = initial_offset - range->offset; /* Part in request.data */
    value.size[1] = range->size - value.size[0]; /* Part in buffer */

    if (value.size[0] > 0) value.p[0] = parser->request.data.p + range->offset;
    if (value.size[1] > 0) value.p[1] = buffer->p + initial_buffer_used + (range->offset - initial_offset);
    
    return value;
}

static struct Error *parse_name_value(struct RequestParser *parser, const struct CharBuffer *buffer, size_t initial_buffer_used)
{
    
    struct Value name, value;
    name = parse_construct_value(parser, buffer, initial_buffer_used, &parser->current_name);
    if (value_compare_case_mem(&name, "content-length", strlen("content-length")))
    {
        value = parse_construct_value(parser, buffer, initial_buffer_used, &parser->current_value);
        value_trim(&value);
        ARET4(value_to_uint(&value, &parser->remaining_content),
            "Invalid integer: %.*s%.*s", (int)value.size[0], value.p[0], (int)value.size[1], value.p[1]);
    }
    else if (value_compare_case_mem(&name, "connection", strlen("connection")))
    {
        struct Value current_value;
        value = parse_construct_value(parser, buffer, initial_buffer_used, &parser->current_value);
        while (true)
        {
            const bool parts_remaining = value_parse_comma(&value, &current_value);
            value_trim(&current_value);
            if (value_compare_case_mem(&current_value, "keep-alive", strlen("keep-alive"))) { parser->request.keep_alive = true; break; }
            if (!parts_remaining) break;
        }
    }
    return OK;
}

struct Error *parse_request(struct RequestParser *parser, const struct CharBuffer *buffer, size_t *buffer_used)
{
    /*
    parse_request() relies on the fact that the algorithm will keep feeding the buffer, processing, sending, and feeding again 
    until the buffer is all used. Therefore no ring structures are needed.
    It may change if output throttling or another limiting mechanism is needed.
    In which case return to initial commit and see how rings were implemented.
    */
    const size_t initial_buffer_used = *buffer_used, initial_offset = parser->request.data.size;
    size_t offset;
    
    if (!initialized) { initialize(); initialized = true; }

    for (offset = initial_offset; *buffer_used < buffer->size; (*buffer_used)++, offset++)
    {
        const char c = buffer->p[*buffer_used];
        const unsigned char c_type = character_map[(unsigned char)c];
        size_t available_content;
        switch (parser->state)
        {
        case RPS_WAIT_METHOD_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_METHOD) != 0)
            {
                parser->request.method.offset = offset;
                parser->request.method.size = 1;
                parser->state = RPS_WAIT_METHOD_END;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;
            
        case RPS_WAIT_METHOD_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_RESOURCE_BEGIN;
            }
            else if ((c_type & CM_METHOD) != 0)
            {
                parser->request.method.size++;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_RESOURCE_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_RESOURCE) != 0)
            {
                parser->request.resource.offset = offset;
                parser->request.resource.size = 1;
                parser->state = RPS_WAIT_RESOURCE_END;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_RESOURCE_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_PROTOCOL_BEGIN;
            }
            else if ((c_type & CM_RESOURCE) != 0)
            {
                parser->request.resource.size++;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_PROTOCOL_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_PROTOCOL) != 0)
            {
                parser->request.protocol.offset = offset;
                parser->request.protocol.size = 1;
                parser->state = RPS_WAIT_PROTOCOL_END;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_PROTOCOL_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_LINE_CR;
            }
            else if ((c_type & CM_PROTOCOL) != 0)
            {
                parser->request.protocol.size++;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r') parser->state = RPS_WAIT_LINE_LF;
                else { parser->tolerated_lf = true; parser->state = RPS_WAIT_NAME_BEGIN; } /* Tolerating LF ending */
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_LINE_CR:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r') parser->state = RPS_WAIT_LINE_LF;
                else { parser->tolerated_lf = true; parser->state = RPS_WAIT_NAME_BEGIN; } /* Tolerating LF ending */
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_LINE_LF:
            if ((c_type & CM_SPACE) != 0) /* Tolerating CR ending */
            {
                parser->tolerated_cr = true;
                parser->state = RPS_WAIT_NAME_BEGIN;
            }
            else if ((c_type & CM_NAME) != 0) /* Tolerating CR ending */
            {
                parser->tolerated_cr = true;
                parser->current_name.offset = offset;
                parser->current_name.size = 1;
                parser->state = RPS_WAIT_NAME_END;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r')
                {
                    parser->tolerated_cr = true;
                    parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received CR CR */
                }
                else
                {
                    parser->state = RPS_WAIT_NAME_BEGIN;
                }
            }
            break;

        case RPS_WAIT_NAME_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_NAME) != 0)
            {
                parser->current_name.offset = offset;
                parser->current_name.size = 1;
                parser->state = RPS_WAIT_NAME_END;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r')
                {
                    if (!parser->tolerated_cr) parser->state = RPS_WAIT_FINAL_LINE_LF; /* Received CR LF CR */
                    else parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received CR CR */
                }
                else
                {
                    parser->tolerated_lf = true;
                    parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received ?? LF */
                }
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;
            
        case RPS_WAIT_NAME_END:
            if ((c_type & CM_SPACE) != 0)
            {
                parser->state = RPS_WAIT_COLON;
            }
            else if ((c_type & CM_NAME) != 0)
            {
                parser->current_name.size++;
            }
            else if (c == ':')
            {
                parser->current_value.offset = offset + 1;
                parser->current_value.size = 0;
                parser->state = RPS_WAIT_VALUE_END;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_COLON:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if (c == ':')
            {
                parser->current_value.offset = offset + 1;
                parser->current_value.size = 0;
                parser->state = RPS_WAIT_VALUE_END;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;
            
        case RPS_WAIT_VALUE_END:
            if ((c_type & CM_VALUE) != 0)
            {
                parser->current_value.size++;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                PRET(parse_name_value(parser, buffer, initial_buffer_used));
                if (c == '\r')
                {
                    if (!parser->tolerated_cr) parser->state = RPS_WAIT_LINE_LF;
                    else parser->state = RPS_WAIT_NAME_BEGIN;
                }
                else
                {
                    parser->tolerated_lf = true;
                    parser->state = RPS_WAIT_NAME_BEGIN;
                }
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_FINAL_LINE_LF:
            if (c == '\n')
            {
                parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END; /* Received CR LF CR LF */
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_CONTENT:
            /* Received the first symbol of centent, processing content in bulk */
            available_content = buffer->size - *buffer_used;
            if (available_content >= parser->remaining_content)
            {
                (*buffer_used) += parser->remaining_content;
                offset += parser->remaining_content;
                parser->state = RPS_END;
            }
            else
            {
                (*buffer_used) += available_content; /* aka (*buffer_used) = buffer->size */
                offset += available_content;
            }
            goto parse_end; /* Breaking out of switch and for without incrementing buffer_used and offset */

        default: /* RPS_END */
            goto parse_end; /* Breaking out of switch and for without incrementing buffer_used and offset */
        }
    }

    parse_end:
    PRET(char_buffer_resize(&parser->request.data, offset));
    memcpy(parser->request.data.p + initial_offset, buffer->p + initial_buffer_used, *buffer_used - initial_buffer_used);
    return OK;
}

void parse_request_initialize(struct RequestParser *parser)
{
    memset((char*)parser, 0, sizeof(struct RequestParser));
}

void parse_request_reset(struct RequestParser *parser)
{
    /* Set everything except request.data to zero */
    const size_t begin = offsetof(struct RequestParser, request.data);
    const size_t end = offsetof(struct RequestParser, request.data) + sizeof(parser->request.data);
    memset((char*)parser, 0, begin);
    memset((char*)parser + end, 0, sizeof(struct RequestParser) - end);
}

void parse_request_finalize(struct RequestParser *parser)
{
    char_buffer_finalize(&parser->request.data);
}
