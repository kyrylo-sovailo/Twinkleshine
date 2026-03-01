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
static bool character_map_initialized = false;

static void initialize_character_map(void)
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
    for (c = '!'; c <= '~'; c++) character_map[(unsigned char)c] |= CM_VALUE; /*All visible ASCII*/
}

static struct Error *parse_name_value(struct RequestParser *parser)
{
    const char *p = parser->request.data.p;
    value_trim(p, &parser->current_name);
    if (value_compare_case_mem(p, &parser->current_name, "content-length", strlen("content-length")))
    {
        value_trim(p, &parser->current_value);
        ARET2(value_to_uint(p, &parser->current_value, &parser->remaining_content),
            "Invalid integer: %.*s", (int)parser->current_value.length, p + parser->current_value.offset);        
    }
    else if (value_compare_case_mem(p, &parser->current_name, "connection", strlen("connection")))
    {
        struct Value current_value_part;
        while (value_parse_comma(p, &parser->current_value, &current_value_part))
        {
            value_trim(p, &current_value_part);
            if (value_compare_case_mem(p, &current_value_part, "keep-alive", strlen("keep-alive"))) { parser->request.keep_alive = true; break; }
        }
    }
    return OK;
}

static struct Error *parse_request_window(struct RequestParser *parser, const struct RingConstWindow *window, size_t *offset)
{
    size_t window_offset, available_content;
    for (window_offset = 0; window_offset < window->size; window_offset++, (*offset)++)
    {
        const char c = window->p[window_offset];
        const unsigned char c_type = character_map[(unsigned char)c];
        switch (parser->state)
        {
        case RPS_WAIT_METHOD_BEGIN:
            if ((c_type & CM_SPACE) != 0)
            {
                /* Do nothing */
            }
            else if ((c_type & CM_METHOD) != 0)
            {
                parser->request.method.offset = *offset;
                parser->request.method.length = 1;
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
                parser->request.method.length++;
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
                parser->request.resource.offset = *offset;
                parser->request.resource.length = 1;
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
                parser->request.resource.length++;
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
                parser->request.protocol.offset = *offset;
                parser->request.protocol.length = 1;
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
                parser->request.resource.length++;
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
                parser->current_name.offset = *offset;
                parser->current_name.length = 1;
                parser->state = RPS_WAIT_NAME_END;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r') /* Received CRCR */
                {
                    parser->tolerated_cr = true;
                    parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END;
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
                parser->current_name.offset = *offset;
                parser->current_name.length = 1;
                parser->state = RPS_WAIT_NAME_END;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                if (c == '\r')
                {
                    if (!parser->tolerated_cr) parser->state = RPS_WAIT_FINAL_LINE_LF;
                    else parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END;
                }
                else
                {
                    parser->tolerated_lf = true;
                    parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END;
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
                parser->current_name.length++;
            }
            else if (c == ':')
            {
                parser->current_value.offset = *offset + 1;
                parser->current_value.length = 0;
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
                parser->current_value.offset = *offset + 1;
                parser->current_value.length = 0;
                parser->state = RPS_WAIT_VALUE_END;
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;
            
        case RPS_WAIT_VALUE_END:
            if ((c_type & CM_VALUE) != 0)
            {
                parser->current_value.length++;
            }
            else if ((c_type & CM_NEWLINE) != 0)
            {
                PRET(parse_name_value(parser));
                if (c == '\r')
                {
                    if (!parser->tolerated_cr) parser->state = RPS_WAIT_FINAL_LINE_LF;
                    else parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END;
                }
                else
                {
                    parser->tolerated_lf = true;
                    parser->state = (parser->remaining_content > 0) ? RPS_WAIT_CONTENT : RPS_END;
                }
            }
            else RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_FINAL_LINE_LF:
            if (c == '\n') RET1("Invalid symbol: %d", (int)c);
            break;

        case RPS_WAIT_CONTENT:
            /* Received the first symbol of centent, processing content in bulk */
            available_content = window->size - window_offset;
            if (available_content >= parser->remaining_content)
            {
                window_offset += parser->remaining_content;
                (*offset) += parser->remaining_content;
                parser->state = RPS_END;
            }
            else
            {
                window_offset += available_content;
                (*offset) += available_content;
            }
            window_offset--; /* Making up for offset++ in for loop (could also just return, the algorithm is guaranteed to exit anyway) */
            (*offset)--;
            break;

        default: /* RPS_END */
            return OK;
        }
    }
    return OK;
}

struct Error *parse_request(struct RequestParser *parser, struct Ring *ring)
{
    struct RingConstWindow windows[2];
    const struct RingConstWindow *window_i;
    unsigned char window_number;
    size_t offset, read;

    if (!character_map_initialized) { initialize_character_map(); character_map_initialized = true; }
    ring_get_all(ring, windows, &window_number);

    /* Parse */
    offset = parser->request.data.size;
    for (window_i = windows; window_i < windows + window_number; window_i++)
    {
        PRET(parse_request_window(parser, window_i, &offset));
        if (parser->state == RPS_END) break;
    }
    read = offset - parser->request.data.size;

    /* Copy data */
    PRET(char_buffer_resize(&parser->request.data, offset));
    offset = parser->request.data.size;
    for (window_i = windows; window_i < windows + window_number && offset < read; window_i++)
    {
        size_t window_read = offset - read;
        if (window_read < window_i->size) window_read = window_i->size;
        memcpy(parser->request.data.p + offset, window_i->p, window_read);
        offset += window_read;
    }

    PRET(ring_pop(ring, read));
    return OK;
}