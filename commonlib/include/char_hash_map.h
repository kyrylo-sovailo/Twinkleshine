#ifndef COMMONLIB_CHAR_HASH_MAP_H
#define COMMONLIB_CHAR_HASH_MAP_H

#include "hash_map.h"

DECLARE_HASH_MAP(char, CharBuffer)
DECLARE_HASH_MAP_INITIALIZE(char, CharBuffer, char_hmap_)
DECLARE_HASH_MAP_FINALIZE(char, CharBuffer, char_hmap_)
DECLARE_HASH_MAP_VALID(char, CharBuffer, char_hmap_)
DECLARE_HASH_MAP_ACCESS(char, CharBuffer, char_hmap_)
DECLARE_HASH_MAP_DELETE(char, CharBuffer, char_hmap_)

#endif
