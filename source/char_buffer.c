#include "../include/char_buffer.h"
#include "../include/buffer_implementation.h"

IMPLEMENT_BUFFER_FINALIZE(char, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_APPEND(char, CharBuffer, char_buffer_)