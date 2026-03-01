#include "../include/output.h"
#include "../include/bool.h"

#include <unistd.h>
#include <sys/stat.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Log filename format: YYYY-MM-DD.log[.N] */
#define MAX_FILE_SIZE (1024 * 1024)         /* Maximum size of log file (1Mb) */
#define MAX_FILE_AGE (7 * 24 * 3600)        /* Maximum age of log file (7 Days) */
#define MAX_TOTAL_SIZE (16 * 1024 * 1024)   /* Maximum total size of log (16 Mb) */
#define MAX_TOTAL_NUMBER 16                 /* Maximum total number of log files (16) */

/* Record about the existence of a log file */
struct Log
{
    time_t global_start;            /* Nominal log start, derived from name */
    unsigned int number;            /* Log number, derived from name */
    unsigned long int size;         /* Log size, bytes */
    char *path;                     /* Full file path */
};
static struct Log logs[MAX_TOTAL_NUMBER];       /* Array of logs, oldest first */
static unsigned int logs_size = 0;              /* Real number of files */
static unsigned long int logs_total_size = 0;   /* Real sum of log file sizes */
static bool initialized = false;                /* Was initialize() called */
static FILE *file = NULL;                       /* Opened file */
static const char *const log_directory = "/var/log/twinkleshine/";              /* Log directory */
static const size_t log_directory_length = sizeof("/var/log/twinkleshine/") - 1;/* Length if log directory */

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
        if (unlink(logs[log_i].path) < 0) success = false;
        free(logs[log_i].path);
        logs_total_size -= logs[log_i].size;
    }
    memmove(&logs[0], &logs[number], (logs_size - number) * sizeof(*logs));
    logs_size -= number;
    return success;
}

/* Delete oldest logs until age and total size are satisfied */
static bool cleanup(time_t global_now)
{
    unsigned long int logs_total_size_copy = logs_total_size;
    unsigned int log_i;
    bool success = true;

    /* Count how many files should be deleted to satisfy constraints */
    for (log_i = 0; log_i < logs_size; log_i++)
    {
        if ((global_now - logs[log_i].global_start) <= MAX_FILE_AGE && logs_total_size_copy <= MAX_TOTAL_SIZE) break;
        logs_total_size_copy -= logs[log_i].size;
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
    for (log_i = 0; log_i < logs_size; log_i++)
    {
        if (log->global_start < logs[logs_size - log_i - 1].global_start) break; /* If new log is older than log i (backwards), break */
    }

    /* Check if the log is really old */
    if (log_i == logs_size)
    {
        if (unlink(logs[log_i].path) < 0) success = false;
        free(logs[log_i].path);
        return success;
    }

    /* Check if logs are full */
    if (logs_size == sizeof(logs) / sizeof(*logs))
    {
        success &= delete(1);
    }

    /* Insert */
    memmove(&logs[logs_size - log_i + 1], &logs[logs_size - log_i], log_i * sizeof(*logs));
    logs[logs_size - log_i] = *log;
    logs_size++;
    logs_total_size += log->size;

    /* Cleanup */
    success &= cleanup(global_now);
    return success;
}

/* Initialize logging, fill logs array */
static bool initialize(void)
{
    time_t global_now = time(NULL);
    DIR *directory;
    struct dirent *entry;
    
    /* Open directory or try to create it */
    directory = opendir(log_directory);
    if (directory == NULL)
    {
        if (mkdir(log_directory, 0755) < 0) return false;
        directory = opendir(log_directory);
    }
    if (directory == NULL) return false;

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
        if (new_log.path == NULL) { closedir(directory); return false; } /* Out of memory, fail */
        memcpy(new_log.path, log_directory, log_directory_length);
        memcpy(new_log.path + log_directory_length, entry->d_name, name_length + 1);
        if (stat(new_log.path, &status) < 0) { free(new_log.path); continue; } /* Stat failed, skip */
        new_log.size = (unsigned long int)status.st_size;
        

        /* Insert */
        if (!insert(global_now, &new_log)) { free(new_log.path); closedir(directory); return false; } /* Insert failed, fail */
        ZERO_AND_FORGET(&new_log);
    }

    closedir(directory);
    return true;
}

void output_open()
{
    time_t global_now, global_start;
    struct tm *p_global_now_calender, global_now_calender, global_start_calender = { 0 };
    bool create_new_log = false;
    unsigned int create_new_log_number = 0;
    
    /* Initialize */
    if (!initialized && !initialize()) exit(1); /* Catastrophic error */
    initialized = true;
    
    /* Get time */
    global_now = time(NULL);
    p_global_now_calender = gmtime(&global_now); /* TODO: not thread-safe */
    if (p_global_now_calender == NULL) exit(2); /* Catastrophic error */
    global_now_calender = *p_global_now_calender;
    global_start_calender.tm_year = global_now_calender.tm_year;
    global_start_calender.tm_mon = global_now_calender.tm_mon;
    global_start_calender.tm_mday = global_now_calender.tm_mday;
    global_start = global_mktime(&global_start_calender, global_now);

    /* Ensure that the last log is the right log */
    if (logs_size == 0 || logs[logs_size - 1].global_start != global_start)
    {
        /* There are no logs or the last log's time does not match */
        create_new_log = true;
    }
    else if (logs[logs_size - 1].size > MAX_FILE_SIZE)
    {
        /* There is a log with matching time, but it is already too big */
        create_new_log_number = logs[logs_size - 1].number + 1;
        create_new_log = true;
    }
    if (create_new_log)
    {
        struct Log new_log = { 0 };
        const size_t name_length = strlen("YYYY-MM-DD.log");
        new_log.global_start = global_start;
        new_log.number = create_new_log_number;
        new_log.path = malloc(log_directory_length + name_length + 1);
        if (new_log.path == NULL) exit(3); /* Catastrophic error */
        memcpy(new_log.path, log_directory, log_directory_length);
        sprintf(new_log.path + log_directory_length, "%u-%u-%u.log", global_start_calender.tm_year + 1900, global_start_calender.tm_mon + 1, global_start_calender.tm_mday);
        if (!insert(global_now, &new_log)) exit(4); /* Catastrophic error */
    }

    /* Write to the last log */
    file = fopen(logs[logs_size - 1].path, "w");
    if (file == NULL) exit(5); /* Catastrophic error */
}

void output_close()
{
    if (file != NULL)
    {
        time_t global_now = time(NULL);
        fclose(file);
        file = NULL;
        cleanup(global_now);
    }
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
    if (file != NULL)
    {
        int result = vfprintf(file, format, va);
        if (result < 0) exit(3); /* Catastrophic error */
        logs[logs_size - 1].size += (unsigned int)result;
        logs_total_size += (unsigned int)result;
    }
}
