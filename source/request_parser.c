#include "../include/request_parser.h"
#include "../include/tables.h"

#include <stdlib.h>
#include <string.h>

static struct Error *parse_name_value(struct RequestParser *parser, const struct Ring *input)
{
    
    struct Value name, value;
    PRET(ring_get(input, &parser->current_name, false, &name));
    if (value_compare_case_mem(&name, "content-length", strlen("content-length")))
    {
        PRET(ring_get(input, &parser->current_value, false, &value));
        value_trim(&value);
        ARET4(value_to_uint(&value, &parser->remaining_content),
            "Invalid integer: %.*s%.*s", (int)value.parts[0].size, value.parts[0].p, (int)value.parts[1].size, value.parts[1].p);
    }
    else if (value_compare_case_mem(&name, "connection", strlen("connection")))
    {
        PRET(ring_get(input, &parser->current_value, false, &value));
        while (true)
        {
            struct Value current_value;
            const bool parts_remaining = value_parse_comma(&value, &current_value);
            value_trim(&current_value);
            if (value_compare_case_mem(&current_value, "keep-alive", strlen("keep-alive"))) { parser->request.keep_alive = true; break; }
            if (!parts_remaining) break;
        }
    }
    return OK;
}

static struct Error *request_parser_part(struct RequestParser *parser, const struct Ring *input, const struct ValuePart *part)
{
    const char *p;
    for (p = part->p; p < part->p + part->size; p++, parser->offset++)
    {
        const char c = *p;
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
                parser->request.method.offset = parser->offset;
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
                parser->request.resource.offset = parser->offset;
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
                parser->request.protocol.offset = parser->offset;
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
                parser->current_name.offset = parser->offset;
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
                parser->current_name.offset = parser->offset;
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
                parser->current_value.offset = parser->offset + 1;
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
                parser->current_value.offset = parser->offset + 1;
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
                PRET(parse_name_value(parser, input));
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
            available_content = part->size - (size_t)(p - part->p);
            if (available_content >= parser->remaining_content)
            {
                p += parser->remaining_content;
                parser->offset += parser->remaining_content;
                parser->state = RPS_END;
            }
            else
            {
                p += available_content; /* aka p = part->p + part->size */
                parser->offset += available_content;
            }
            return OK; /* Breaking out of switch and for without incrementing offset */

        default: /* RPS_END */
            return OK; /* Breaking out of switch and for without incrementing offset */
        }
    }
    return OK;
}

struct Error *request_parser_parse(struct RequestParser *parser, const struct Ring *input)
{
    struct ValueLocation unused_location;
    struct Value unused;
    unsigned char i;
    
    unused_location.offset = parser->offset;
    unused_location.size = input->size - parser->offset;
    PRET(ring_get(input, &unused_location, false, &unused));
    for (i = 0; i < VALUE_PARTS; i++)
    {
        PRET(request_parser_part(parser, input, &unused.parts[i]));
    }
    return OK;
}