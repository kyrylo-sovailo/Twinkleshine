#ifndef COMMONLIB_CHAR_BUFFER_H
#define COMMONLIB_CHAR_BUFFER_H

#include "buffer.h"
#include "char.h"

DECLARE_BUFFER(cchar_t, CharBuffer)
DECLARE_BUFFER_INITIALIZE(cchar_t, CharBuffer, char_buffer_)
DECLARE_BUFFER_FINALIZE(cchar_t, CharBuffer, char_buffer_)
DECLARE_BUFFER_RESIZE(cchar_t, CharBuffer, char_buffer_)
DECLARE_BUFFER_RESERVE(cchar_t, CharBuffer, char_buffer_)
DECLARE_BUFFER_APPEND(cchar_t, CharBuffer, char_buffer_)
DECLARE_BUFFER_PUSH(cchar_t, CharBuffer, char_buffer_)

#endif
