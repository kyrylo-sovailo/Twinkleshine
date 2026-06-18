#ifndef TABLES_H
#define TABLES_H

enum CharacterMap
{
    CM_NONE = 0,

    /* HTTP-related */
    CM_SPACE = 1,
    CM_NEWLINE = 2,
    CM_METHOD = 4,
    CM_RESOURCE = 8,
    CM_PROTOCOL = 16,
    CM_NAME = 32,
    CM_VALUE = 64,
    CM_DOMAIN = 128
};
extern unsigned char character_map[256];

extern const char *months_xxx[12];
extern const char *days_xxx[7];

extern const char *fun_facts[1002];
extern const char *fun_facts_de[269];

void tables_module_initialize(void);

#endif
