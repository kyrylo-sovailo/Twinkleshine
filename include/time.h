#ifndef TIME_H
#define TIME_H

#include "../commonlib/include/bool.h"
#include <time.h>

/* Initializes time module */
void time_module_initialize(void);

/* gmtime()/localtime() except C89 and more intuitive */
time_t calender_to_time(struct tm *global_calender, time_t probe);

/* mktime() except C89 and more intuitive */
struct tm time_to_calender(time_t global_time, bool global);

/* Human-readable time since startup */
void time_to_string_uptime(char buffer[64], char language);

#endif
