#ifndef ERROR_H
#define ERROR_H

#include "bool.h"
#include "macro.h"

/*#define ERROR_EMBED_ARGUMENTS*/ /* Store file and line as a part of the message (increases file size) */
#define ERROR_INCLUDE_EXPRESSION /* Include boolean expression in the trace */

#ifdef ERROR_EMBED_ARGUMENTS
    #define ERROR_FORMAT() __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Error"
    #define ERROR_FORMAT_F(FORMAT) __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Message: " FORMAT
    #ifdef ERROR_INCLUDE_EXPRESSION
        #define ERROR_FORMAT_E(EXPRESSION) __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Condition `" EXPRESSION "' failed"
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) __BASENAME_FILE__ ":" ERROR_STRINGIZE_LINE ": Condition `" EXPRESSION "' failed. Message: " FORMAT
    #else
        #define ERROR_FORMAT_E(EXPRESSION) ERROR_FORMAT()
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) ERROR_FORMAT_F(FORMAT)
    #endif
#else
    #define ERROR_FORMAT() "%s:%d: Error", __BASENAME_FILE__, __LINE__
    #define ERROR_FORMAT_F(FORMAT) "%s:%d: Message: " FORMAT, __BASENAME_FILE__, __LINE__
    #ifdef ERROR_INCLUDE_EXPRESSION
        #define ERROR_FORMAT_E(EXPRESSION) "%s:%d: Condition `%s' failed", __BASENAME_FILE__, __LINE__, EXPRESSION
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) "%s:%d: Condition `%s' failed. Message: " FORMAT, __BASENAME_FILE__, __LINE__, EXPRESSION
    #else
        #define ERROR_FORMAT_E(EXPRESSION) ERROR_FORMAT()
        #define ERROR_FORMAT_EF(EXPRESSION, FORMAT) ERROR_FORMAT_F(FORMAT)
    #endif
#endif

struct Error;
#define OK ((struct Error*)0)
#define PANIC ((struct Error*)1)

/* Assigns 'error' variable and goes to 'finalize' label (GOTO = goto) */
#define GOTO() { error = error_internal_allocate(ERROR_FORMAT()); goto failure; }
#define GOTO0(FORMAT) { error = error_internal_allocate(ERROR_FORMAT_F(FORMAT)); goto failure; }
#define GOTO1(FORMAT, A) { error = error_internal_allocate(ERROR_FORMAT_F(FORMAT), A); goto failure; }
#define GOTO2(FORMAT, A, B) { error = error_internal_allocate(ERROR_FORMAT_F(FORMAT), A, B); goto failure; }
#define GOTO3(FORMAT, A, B, C) { error = error_internal_allocate(ERROR_FORMAT_F(FORMAT), A, B, C); goto failure; }
#define GOTO4(FORMAT, A, B, C, D) { error = error_internal_allocate(ERROR_FORMAT_F(FORMAT), A, B, C, D); goto failure; }

/* Assigns 'error' variable and goes to 'finalize' label if expression is false (AGOTO = conditional goto) */
#define AGOTO(EXPRESSION) { const bool check = EXPRESSION; if (!check) { error = error_internal_allocate(ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define AGOTO0(EXPRESSION, FORMAT) { const bool check = EXPRESSION; if (!check) { error = error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); goto failure; } }
#define AGOTO1(EXPRESSION, FORMAT, A) { const bool check = EXPRESSION; if (!check) { error = error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A); goto failure; } }
#define AGOTO2(EXPRESSION, FORMAT, A, B) { const bool check = EXPRESSION; if (!check) { error = error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B); goto failure; } }
#define AGOTO3(EXPRESSION, FORMAT, A, B, C) { const bool check = EXPRESSION; if (!check) { error = error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C); goto failure; } }
#define AGOTO4(EXPRESSION, FORMAT, A, B, C, D) { const bool check = EXPRESSION; if (!check) { error = error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C, D); goto failure; } }

/* Assigns 'error' variable and goes to 'finalize' label if expression is error (PGOTO = propagate goto) */
#define PGOTO(EXPRESSION) { struct Error *check = EXPRESSION; if (check != OK) { error = error_internal_allocate_append(check, ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define PGOTO0(EXPRESSION, FORMAT) { struct Error *check = EXPRESSION; if (check != OK) { error = error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); goto failure; } }
#define PGOTO1(EXPRESSION, FORMAT, A) { struct Error *check = EXPRESSION; if (check != OK) { error = error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A); goto failure; } }
#define PGOTO2(EXPRESSION, FORMAT, A, B) { struct Error *check = EXPRESSION; if (check != OK) { error = error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B); goto failure; } }
#define PGOTO3(EXPRESSION, FORMAT, A, B, C) { struct Error *check = EXPRESSION; if (check != OK) { error = error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C); goto failure; } }
#define PGOTO4(EXPRESSION, FORMAT, A, B, C, D) { struct Error *check = EXPRESSION; if (check != OK) { error = error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C, D); goto failure; } }

/* Returns the error (RET = return) */
#define RET() { return error_internal_allocate(ERROR_FORMAT()); }
#define RET0(FORMAT) { return error_internal_allocate(ERROR_FORMAT_F(FORMAT)); }
#define RET1(FORMAT, A) { return error_internal_allocate(ERROR_FORMAT_F(FORMAT), A); }
#define RET2(FORMAT, A, B) { return error_internal_allocate(ERROR_FORMAT_F(FORMAT), A, B); }
#define RET3(FORMAT, A, B, C) { return error_internal_allocate(ERROR_FORMAT_F(FORMAT), A, B, C); }
#define RET4(FORMAT, A, B, C, D) { return error_internal_allocate(ERROR_FORMAT_F(FORMAT), A, B, C, D); }

/* Returns error if expression is false (ARET = conditional return) */
#define ARET(EXPRESSION) { const bool check = EXPRESSION; if (!check) return error_internal_allocate(ERROR_FORMAT_E(#EXPRESSION)); }
#define ARET0(EXPRESSION, FORMAT) { const bool check = EXPRESSION; if (!check) return error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); }
#define ARET1(EXPRESSION, FORMAT, A) { const bool check = EXPRESSION; if (!check) return error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A); }
#define ARET2(EXPRESSION, FORMAT, A, B) { const bool check = EXPRESSION; if (!check) return error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B); }
#define ARET3(EXPRESSION, FORMAT, A, B, C) { const bool check = EXPRESSION; if (!check) return error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C); }
#define ARET4(EXPRESSION, FORMAT, A, B, C, D) { const bool check = EXPRESSION; if (!check) return error_internal_allocate(ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C, D); }

/* Returns error if expression is error (PRET = propagate return) */
#define PRET(EXPRESSION) { struct Error *check = EXPRESSION; if (check != OK) return error_internal_allocate_append(check, ERROR_FORMAT_E(#EXPRESSION)); }
#define PRET0(EXPRESSION, FORMAT) { struct Error *check = EXPRESSION; if (check != OK) return error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); }
#define PRET1(EXPRESSION, FORMAT, A) { struct Error *check = EXPRESSION; if (check != OK) return error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A); }
#define PRET2(EXPRESSION, FORMAT, A, B) { struct Error *check = EXPRESSION; if (check != OK) return error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B); }
#define PRET3(EXPRESSION, FORMAT, A, B, C) { struct Error *check = EXPRESSION; if (check != OK) return error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C); }
#define PRET4(EXPRESSION, FORMAT, A, B, C, D) { struct Error *check = EXPRESSION; if (check != OK) return error_internal_allocate_append(check, ERROR_FORMAT_EF(#EXPRESSION, FORMAT), A, B, C, D); }

/* Creates error (guaranteed to succeed) */
struct Error *error_internal_allocate(const char *format, ...) NODISCARD PRINTFLIKE(1, 2);

/* Appends error to an existing error (guaranteed to succeed) */
struct Error *error_internal_allocate_append(struct Error *error, const char *format, ...) NODISCARD PRINTFLIKE(2, 3);

/* Gets error code to be returned by application (guaranteed to succeed) */
int error_get_exit_code(const struct Error *error);

/* Prints error (guaranteed to succeed) */
void error_print(const struct Error *error);

/* Finalizes error (guaranteed to succeed) */
void error_finalize(struct Error *error);

#endif
