#ifndef CHAR_BUFFER_H
#define CHAR_BUFFER_H

#include "buffer.h"

DECLARE_BUFFER(char, CharBuffer)
DECLARE_BUFFER_FINALIZE(char, CharBuffer, char_buffer_)
DECLARE_BUFFER_APPEND(char, CharBuffer, char_buffer_)
DECLARE_BUFFER_RESIZE(char, CharBuffer, char_buffer_)

#endif
