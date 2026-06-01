#include "../include/time.h"
#include "../commonlib/include/macro.h"

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
