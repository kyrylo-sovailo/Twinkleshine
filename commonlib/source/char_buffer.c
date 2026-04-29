#include "../include/char_buffer.h"
#include "../include/buffer_implementation.h"

static void char_buffer_initialize_element(cchar_t *p) { (void)p; }
static void char_buffer_finalize_element(cchar_t *p) { (void)p; }

IMPLEMENT_BUFFER_INITIALIZE(cchar_t, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_FINALIZE(cchar_t, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_RESIZE(cchar_t, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_RESERVE(cchar_t, CharBuffer, char_buffer_)
IMPLEMENT_BUFFER_APPEND(cchar_t, CharBuffer, char_buffer_)
#ifdef COMMON_WCHAR
	IMPLEMENT_BUFFER_PUSH(cchar_t, CharBuffer, char_buffer_, 2)
#else
	IMPLEMENT_BUFFER_PUSH(cchar_t, CharBuffer, char_buffer_, 1)
#endif
