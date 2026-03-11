#include "../include/tables.h"

#include <stddef.h>
#include <string.h>

unsigned char character_map[256];

const char *months_xxx[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char *days_xxx[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void tables_initialize(void)
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