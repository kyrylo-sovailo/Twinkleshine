#ifndef EXTENDED_ERROR_H
#define EXTENDED_ERROR_H

#include "../commonlib/include/bool.h"
#include "../commonlib/include/char.h"
#include "../commonlib/include/macro.h"

struct Error;

/* Action that should be taken in error handling */
enum ExErrorFlag
{
    EEF_SEND    = 1,    /* Send fixed message to the client */
    EEF_CLOSE   = 2,    /* Close the connection */
    EEF_LOG     = 4,    /* Log the incident */
    EEF_DIE     = 8     /* Total failure */
};

#define EEF_CLOSE_LOG EEF_CLOSE | EEF_LOG
#define EEF_CLOSE_LOG_DIE EEF_CLOSE | EEF_LOG | EEF_DIE
#define EEF_SEND_CLOSE_LOG(FIXED_RESPONSE) EEF_SEND | EEF_CLOSE | EEF_LOG | FIXED_RESPONSE

struct ExError { struct Error *error; enum ExErrorFlag flags; };

struct ExError exerror_internal_allocate(enum ExErrorFlag flags, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
struct ExError exerror_internal_allocate_append(struct ExError exerror, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
struct ExError exerror_internal_allocate_append_extend(struct Error *error, enum ExErrorFlag flags, const cchar_t *format, ...) NODISCARD PRINTFLIKE(3, 4);

#define EXRET(FLAGS) { return exerror_internal_allocate(FLAGS, ERROR_FORMAT()); }
#define EXRET0(FORMAT, FLAGS) { return exerror_internal_allocate(FLAGS, ERROR_FORMAT_F(FORMAT)); }
#define EXRET1(FORMAT, A, FLAGS) { return exerror_internal_allocate(FLAGS, ERROR_FORMAT_F(FORMAT), A); }
#define EXAGOTO(EXPRESSION, FLAGS) { const bool check = EXPRESSION; if (check) {} else { exerror = exerror_internal_allocate(FLAGS, ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define EXAGOTO0(EXPRESSION, FORMAT, FLAGS) { const bool check = EXPRESSION; if (check) {} else { exerror = exerror_internal_allocate(FLAGS, ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); goto failure; } }
#define EXARET(EXPRESSION, FLAGS) { const bool check = EXPRESSION; if (check) {} else return exerror_internal_allocate(FLAGS, ERROR_FORMAT_E(#EXPRESSION)); }
#define EXARET0(EXPRESSION, FORMAT, FLAGS) { const bool check = EXPRESSION; if (check) {} else return exerror_internal_allocate(FLAGS, ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); }
#define EXPGOTO(EXPRESSION) { struct ExError check = EXPRESSION; if (check.error != OK) { exerror = exerror_internal_allocate_append(check, ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define EXPGOTOF(EXPRESSION, FLAGS) { struct Error *check = EXPRESSION; if (check != OK) { exerror = exerror_internal_allocate_append_extend(check, FLAGS, ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define EXPRET(EXPRESSION) { struct ExError check = EXPRESSION; if (check.error != OK) return exerror_internal_allocate_append(check, ERROR_FORMAT_E(#EXPRESSION)); }
#define EXPRETF(EXPRESSION, FLAGS) { struct Error *check = EXPRESSION; if (check != OK) return exerror_internal_allocate_append_extend(check, FLAGS, ERROR_FORMAT_E(#EXPRESSION)); }
#define EXPIGNORE(EXPRESSION) { struct ExError check = EXPRESSION; if (check.error != OK) {} else {} }

#endif
