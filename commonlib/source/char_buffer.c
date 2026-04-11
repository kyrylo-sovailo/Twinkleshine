#include "../include/char_buffer.h"
#include "../include/buffer_implementation.h"

static void char_buffer_initialize_element(char *p) { (void)p; }
static void char_buffer_finalize_element(char *p) { (void)p; }

IMPLEMENT_BUFFER_INITIALIZE(char, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_FINALIZE(char, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_RESIZE(char, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_RESERVE(char, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_APPEND(char, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_PUSH(char, CharBuffer, char_buffer_, 1)

#ifdef WIN32

static void wchar_buffer_initialize_element(wchar_t *p) { (void)p; }
static void wchar_buffer_finalize_element(wchar_t *p) { (void)p; }

IMPLEMENT_BUFFER_INITIALIZE(wchar_t, WCharBuffer, wchar_buffer_)
IMPLEMENT_BUFFER_FINALIZE(wchar_t, WCharBuffer, wchar_buffer_)
IMPLEMENT_BUFFER_RESIZE(wchar_t, WCharBuffer, wchar_buffer_)
IMPLEMENT_BUFFER_RESERVE(wchar_t, WCharBuffer, wchar_buffer_)
IMPLEMENT_BUFFER_APPEND(wchar_t, WCharBuffer, wchar_buffer_)
IMPLEMENT_BUFFER_PUSH(wchar_t, WCharBuffer, wchar_buffer_, 2)

#endif
