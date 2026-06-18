#include "../include/time.h"
#include "../commonlib/include/macro.h"
#include "../include/constants.h"

#include <stdio.h>
#include <string.h>

static time_t g_startup_time;

void time_module_initialize(void)
{
    g_startup_time= time(NULL);
}

struct tm time_to_calender(time_t global_time, bool global)
{
    struct tm *p_calender = global ? gmtime(&global_time) : localtime(&global_time); /* TODO: not thread-safe */
    if (p_calender != NULL)
    {
        return *p_calender;
    }
    else
    {
        struct tm calender = ZERO_INIT;
        calender.tm_year = 70;
        calender.tm_mday = 1;
        calender.tm_wday = 4; /* Thursday */
        return calender;
    }
}

time_t calender_to_time(struct tm *global_calender, time_t probe)
{
    time_t local_time, offset;
    struct tm probe_global_calender, probe_local_calender;
    local_time = mktime(global_calender);
    if (local_time == (time_t)-1) return local_time;
    probe_global_calender = time_to_calender(probe, true);
    probe_local_calender = time_to_calender(probe, false);
    offset = mktime(&probe_local_calender) - mktime(&probe_global_calender);
    return local_time + offset;
}

void time_to_string_uptime(char buffer[64], char language)
{
    const time_t now = time(NULL);
    const time_t difference = (now < g_startup_time) ? 0 : (now - g_startup_time);
    const unsigned int days = (unsigned int)(difference / DAY);
    const unsigned int hours = (unsigned int)(difference / HOUR) % 24;
    const unsigned int minutes = (unsigned int)(difference / MINUTE) % 60;
    const unsigned int seconds = (unsigned int)(difference / SECOND) % 60;
    const char *days_str, *hours_str, *minutes_str, *seconds_str, *and_str, *serial_and_str;

    if (language == 'd')
    {
        days_str    = (days == 1)   ? "Tag"     : "Tage";
        hours_str   = (hours == 1)  ? "Stunde"  : "Stunden";
        minutes_str = (minutes == 1)? "Minute"  : "Minuten";
        seconds_str = (seconds == 1)? "Sekunde" : "Sekunden";
        and_str = serial_and_str = " und ";
    }
    else
    {
        days_str    = (days == 1)   ? "day"     : "days";
        hours_str   = (hours == 1)  ? "hour"    : "hours";
        minutes_str = (minutes == 1)? "minute"  : "minutes";
        seconds_str = (seconds == 1)? "second"  : "seconds";
        and_str = " and ";
        serial_and_str = ", and ";
    }

    if (days > 0) sprintf(buffer, "%u %s, %u %s, %u %s%s%u %s", days, days_str, hours, hours_str, minutes, minutes_str, serial_and_str, seconds, seconds_str);
    else if (hours > 0) sprintf(buffer, "%u %s, %u %s%s%u %s", hours, hours_str, minutes, minutes_str, serial_and_str, seconds, seconds_str);
    else if (minutes > 0) sprintf(buffer, "%u %s%s%u %s", minutes, minutes_str, and_str, seconds, seconds_str);
    else /* if (seconds > 0) */ sprintf(buffer, "%u %s", seconds, seconds_str);
}
