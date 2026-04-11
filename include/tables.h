#ifndef TABLES_H
#define TABLES_H

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
extern unsigned char character_map[256];

extern const char *months_xxx[12];
extern const char *days_xxx[7];

void tables_module_initialize(void);

#endif
