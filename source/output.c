#include "../include/output.h"
#include "../include/bool.h"
#include "../include/constants.h"

#include <unistd.h>
#include <sys/stat.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Log filename format: YYYY-MM-DD.log[.N] */

/* Record about the existence of a log file */
struct Log
{
    time_t global_start;            /* Nominal log start, derived from name */
    unsigned int number;            /* Log number, derived from name */
    unsigned long int size;         /* Log size, bytes */
    char *path;                     /* Full file path */
};
static const char *const log_directory = "/var/log/twinkleshine/";              /* Log directory */
static const size_t log_directory_length = sizeof("/var/log/twinkleshine/") - 1;/* Length if log directory */
static struct Log g_logs[MAX_TOTAL_LOG_NUMBER]; /* Array of logs, oldest first */
static unsigned int g_logs_size = 0;            /* Real number of files */
static unsigned long int g_logs_total_size = 0; /* Real sum of log file sizes */
static bool g_catastrophic = false;             /* Initialization failed, fall back to catastrophic output */
static FILE *g_file = NULL;                     /* Opened file */

/* Transforms global calender to global time (aka timegm) */
static time_t global_mktime(struct tm *global_calender, time_t probe)
{
    struct tm *p_probe_global_calender, probe_global_calender;
    struct tm *p_probe_local_calender, probe_local_calender;
    time_t offset = 0;
    p_probe_global_calender = gmtime(&probe); /* TODO: not thread-safe */
    if (p_probe_global_calender != NULL) probe_global_calender = *p_probe_global_calender;
    p_probe_local_calender = localtime(&probe);
    if (p_probe_local_calender != NULL) probe_local_calender = *p_probe_local_calender;
    if (p_probe_global_calender != NULL && p_probe_local_calender != NULL) offset = mktime(&probe_local_calender) - mktime(&probe_global_calender);
    return mktime(global_calender) + offset;
}

/* Deletes N oldest files */
static bool delete(unsigned int number)
{
    unsigned int log_i;
    bool success = true;

    for (log_i = 0; log_i < number; log_i++)
    {
        if (unlink(g_logs[log_i].path) < 0) success = false;
        free(g_logs[log_i].path);
        g_logs[log_i].path = NULL;
        g_logs_total_size -= g_logs[log_i].size;
    }
    memmove(&g_logs[0], &g_logs[number], (g_logs_size - number) * sizeof(*g_logs));
    g_logs_size -= number;
    return success;
}

/* Delete oldest logs until age and total size are satisfied */
static bool cleanup(time_t global_now)
{
    unsigned long int logs_total_size_copy = g_logs_total_size;
    unsigned int log_i;
    bool success = true;

    /* Count how many files should be deleted to satisfy constraints */
    for (log_i = 0; log_i < g_logs_size; log_i++)
    {
        if ((global_now - g_logs[log_i].global_start) <= MAX_LOG_AGE && logs_total_size_copy <= MAX_TOTAL_LOG_SIZE) break;
        logs_total_size_copy -= g_logs[log_i].size;
    }

    /* Delete */
    success &= delete(log_i);
    return success;
}

/* Inserts new log into log array or deletes it if it is too old */
static bool insert(time_t global_now, struct Log *log)
{
    bool success = true;
    unsigned int log_i;

    /* Count how many logs is the new log older than */
    for (log_i = 0; log_i < g_logs_size; log_i++)
    {
        if (log->global_start < g_logs[g_logs_size - log_i - 1].global_start) break; /* If new log is older than log i (backwards), break */
    }

    /* Check if the log is really old */
    if (log_i == g_logs_size)
    {
        if (unlink(g_logs[log_i].path) < 0) success = false;
        free(g_logs[log_i].path);
        g_logs[log_i].path = NULL;
        return success;
    }

    /* Check if logs are full */
    if (g_logs_size == sizeof(g_logs) / sizeof(*g_logs))
    {
        success &= delete(1);
    }

    /* Insert */
    memmove(&g_logs[g_logs_size - log_i + 1], &g_logs[g_logs_size - log_i], log_i * sizeof(*g_logs));
    g_logs[g_logs_size - log_i] = *log; /* TODO: ensure no double free is possible */
    g_logs_size++;
    g_logs_total_size += log->size;

    /* Cleanup */
    success &= cleanup(global_now);
    return success;
}

void output_initialize(void)
{
    time_t global_now = time(NULL);
    DIR *directory;
    struct dirent *entry;
    
    /* Open directory or try to create it */
    directory = opendir(log_directory);
    if (directory == NULL)
    {
        if (mkdir(log_directory, 0755) < 0) { g_catastrophic = true; return; }
        directory = opendir(log_directory);
    }
    if (directory == NULL) { g_catastrophic = true; return; }

    /* Read directory */
    for (entry = readdir(directory); entry != NULL; entry = readdir(directory))
    {
        const char *p;
        struct tm global_start_calender = { 0 };
        char *next_p;
        struct Log new_log = { 0 };
        size_t name_length;
        struct stat status;

        /* Skip non-files */
        if ((entry->d_type & 8 /*DT_REG*/) != 0) continue;
        
        /* Parse date */
        p = entry->d_name;
        global_start_calender.tm_year = (int)strtol(p, &next_p, 10) - 1900;
        if (next_p == p || *next_p != '-') continue; /* Year read incorrectly, skip */
        p = next_p + 1;
        global_start_calender.tm_mon = (int)strtol(p, &next_p, 10) - 1;
        if (next_p == p || *next_p != '-') continue; /* Month read incorrectly, skip */
        p = next_p + 1;
        global_start_calender.tm_mday = (int)strtol(p, &next_p, 10);
        if (next_p == p || *next_p != '.') continue; /* Day read incorrectly, skip */
        p = next_p + 1;
        new_log.global_start = global_mktime(&global_start_calender, global_now);
        if (new_log.global_start == ((time_t)-1)) continue; /* Time read incorrectly, skip */

        /* Parse extension */
        if (strcmp(p, "log") != 0) continue; /* Extension incorrect, skip */
        p += 3;

        /* Parse number */
        if (*p == '\0')
        {
            new_log.number = 0;
        }
        else if (*p == '.')
        {
            p++;
            new_log.number = (unsigned int)strtoul(p, &next_p, 10);
            if (next_p == p || *next_p != '\0') continue; /* Number read incorrectly, skip */
        }
        else continue; /* Number read incorrectly, skip */

        /* Read size */
        name_length = strlen(entry->d_name);
        new_log.path = malloc(log_directory_length + name_length + 1);
        if (new_log.path == NULL) { closedir(directory); g_catastrophic = true; return; } /* Out of memory, fail */
        memcpy(new_log.path, log_directory, log_directory_length);
        memcpy(new_log.path + log_directory_length, entry->d_name, name_length + 1);
        if (stat(new_log.path, &status) < 0) { free(new_log.path); continue; } /* Stat failed, skip */
        new_log.size = (unsigned long int)status.st_size;
        

        /* Insert */
        if (!insert(global_now, &new_log)) { free(new_log.path); closedir(directory); g_catastrophic = true; return; } /* Insert failed, fail */
        ZERO_AND_FORGET(&new_log);
    }

    closedir(directory);
}

void output_finalize(void)
{
    unsigned int log_i;
    for (log_i = 0; log_i < g_logs_size; log_i++)
    {
        if (g_logs[log_i].path != NULL) free(g_logs[log_i].path);
    }
    if (g_file != NULL) fclose(g_file);
}

void output_open(void)
{
    time_t global_now, global_start;
    struct tm *p_global_now_calender, global_now_calender, global_start_calender = { 0 };
    bool create_new_log = false;
    unsigned int create_new_log_number = 0;
    
    if (g_catastrophic) return;
    
    /* Get time */
    global_now = time(NULL);
    p_global_now_calender = gmtime(&global_now); /* TODO: not thread-safe */
    if (p_global_now_calender == NULL) { g_catastrophic = true; return; }
    global_now_calender = *p_global_now_calender;
    global_start_calender.tm_year = global_now_calender.tm_year;
    global_start_calender.tm_mon = global_now_calender.tm_mon;
    global_start_calender.tm_mday = global_now_calender.tm_mday;
    global_start = global_mktime(&global_start_calender, global_now);

    /* Ensure that the last log is the right log */
    if (g_logs_size == 0 || g_logs[g_logs_size - 1].global_start != global_start)
    {
        /* There are no logs or the last log's time does not match */
        create_new_log = true;
    }
    else if (g_logs[g_logs_size - 1].size > MAX_LOG_SIZE)
    {
        /* There is a log with matching time, but it is already too big */
        create_new_log_number = g_logs[g_logs_size - 1].number + 1;
        create_new_log = true;
    }
    if (create_new_log)
    {
        struct Log new_log = { 0 };
        const size_t name_length = strlen("YYYY-MM-DD.log");
        new_log.global_start = global_start;
        new_log.number = create_new_log_number;
        new_log.path = malloc(log_directory_length + name_length + 1);
        if (new_log.path == NULL) { g_catastrophic = true; return; }
        memcpy(new_log.path, log_directory, log_directory_length);
        sprintf(new_log.path + log_directory_length, "%u-%u-%u.log", global_start_calender.tm_year + 1900, global_start_calender.tm_mon + 1, global_start_calender.tm_mday);
        if (!insert(global_now, &new_log)) { free(new_log.path); g_catastrophic = true; return; }
        ZERO_AND_FORGET(&new_log);
    }

    /* Write to the last log */
    g_file = fopen(g_logs[g_logs_size - 1].path, "w");
    if (g_file == NULL) { g_catastrophic = true; return; }
}

void output_close(void)
{
    time_t global_now;
    if (g_catastrophic) return; /* Catastrophic, nothing to do */
    if (g_file == NULL) return; /* Already closed, nothing to do */
    global_now = time(NULL);
    fclose(g_file);
    g_file = NULL;
    cleanup(global_now);
}

void output_print(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    output_vprint(format, va);
    va_end(va);
}

void output_vprint(const char *format, va_list va)
{
    if (g_catastrophic)
    {
        int result = vfprintf(stderr, format, va);
        if (result < 0) exit(1); /* Nothing can be done, fatal error */
    }
    else if (g_file != NULL)
    {
        int result = vfprintf(g_file, format, va);
        if (result < 0) { g_catastrophic = true; output_print("ERROR LOST"); return; }
        g_logs[g_logs_size - 1].size += (unsigned int)result;
        g_logs_total_size += (unsigned int)result;
    }
}

void output_print_time(void)
{
    time_t global_time;
    struct tm *p_global_calender, global_calender;
    struct tm *p_local_calender, local_calender;
    char calender_buffer[64];

    /* Print time */
    global_time = time(NULL);
    p_global_calender = gmtime(&global_time); /* TODO: not thread-safe btw */
    if (p_global_calender != NULL) global_calender = *p_global_calender;
    p_local_calender = localtime(&global_time);
    if (p_local_calender != NULL) local_calender = *p_local_calender;
    if (p_global_calender != NULL)
    {
        strftime(calender_buffer, sizeof(calender_buffer), "%a, %d %b %Y %H:%M:%S %Z", &global_calender);
        output_print("%s\n", calender_buffer);
        if (p_local_calender != NULL)
        {
            strftime(calender_buffer, sizeof(calender_buffer), "%a, %d %b %Y %H:%M:%S %Z", &local_calender);
            output_print("%s\n", calender_buffer);
        }
    }
}
