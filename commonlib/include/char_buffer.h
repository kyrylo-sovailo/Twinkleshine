#ifndef COMMONLIB_CHAR_BUFFER_H
#define COMMONLIB_CHAR_BUFFER_H

#include "buffer.h"

DECLARE_BUFFER(char, CharBuffer)
DECLARE_BUFFER_INITIALIZE(char, CharBuffer, char_buffer_)
DECLARE_BUFFER_FINALIZE(char, CharBuffer, char_buffer_)
DECLARE_BUFFER_RESIZE(char, CharBuffer, char_buffer_)
DECLARE_BUFFER_RESERVE(char, CharBuffer, char_buffer_)
DECLARE_BUFFER_APPEND(char, CharBuffer, char_buffer_)
DECLARE_BUFFER_PUSH(char, CharBuffer, char_buffer_)

#ifdef WIN32

DECLARE_BUFFER(wchar_t, WCharBuffer)
DECLARE_BUFFER_INITIALIZE(wchar_t, WCharBuffer, wchar_buffer_)
DECLARE_BUFFER_FINALIZE(wchar_t, WCharBuffer, wchar_buffer_)
DECLARE_BUFFER_RESIZE(wchar_t, WCharBuffer, wchar_buffer_)
DECLARE_BUFFER_RESERVE(wchar_t, WCharBuffer, wchar_buffer_)
DECLARE_BUFFER_APPEND(wchar_t, WCharBuffer, wchar_buffer_)
DECLARE_BUFFER_PUSH(wchar_t, WCharBuffer, wchar_buffer_)

#endif

#endif
