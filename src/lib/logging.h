/**
 * Utility functions.
 */

#ifndef DCCS_LOGGING_H
#define DCCS_LOGGING_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 1

#ifdef MPI_VERSION
extern int mpi_rank;
#endif

#if USE_PTHREAD
#include <pthread.h>
static pthread_mutex_t log_mutex;
static bool log_mutex_init = false;
#endif

/* Logging functions */

#define LOG_VERBOSE 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_WARNING 3
#define LOG_ERROR 4
#define LOG_PERROR 5

void vlog_level(int level, const char *format, va_list arg) {
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#if USE_PTHREAD
    if (!log_mutex_init) {
        pthread_mutex_init(&log_mutex, NULL);
        log_mutex_init = true;
    }

    pthread_mutex_lock(&log_mutex);
#endif

    switch (level) {
#ifdef MPI_VERSION
    #define PRINTTAG(name, color) \
        fprintf(stderr, "[%d:%s%s%s]\t", mpi_rank, (color), (name), KNRM)
#else
    #define PRINTTAG(name, color) \
        fprintf(stderr, "[%s%s%s]\t", (color), (name), KNRM)
#endif
#define PRINTMSG(name, color) \
    { \
        PRINTTAG((name), (color)); \
        vfprintf(stderr, format, arg); \
    }
        case LOG_VERBOSE:
            PRINTMSG("verbose", KBLU);
            break;
        case LOG_INFO:
            PRINTMSG("info", KCYN);
            break;
        case LOG_DEBUG:
#if DEBUG
            PRINTMSG("debug", KMAG);
#endif
            break;
        case LOG_WARNING:
            PRINTMSG("warn", KYEL);
            break;
        case LOG_ERROR:
            PRINTMSG("error", KRED);
            break;
        case LOG_PERROR:
            PRINTTAG("error", KRED);
            perror(format);
            break;
        default: {
            char level_str[32];
            bool success = sprintf(level_str, "%d", level) > 0;
            PRINTMSG(success ? level_str : "unknown", KBLU);
            break;
        }
#undef PRINTMSG
#undef PRINTTAG
    }

#if USE_PTHREAD
    pthread_mutex_unlock(&log_mutex);
#endif
}

void llog(int level, const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vlog_level(level, format, arg);
    va_end(arg);
}

void log_verbose(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vlog_level(LOG_VERBOSE, format, arg);
    va_end(arg);
}

void log_info(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vlog_level(LOG_INFO, format, arg);
    va_end(arg);
}

void log_debug(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vlog_level(LOG_DEBUG, format, arg);
    va_end(arg);
}

void log_warning(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vlog_level(LOG_WARNING, format, arg);
    va_end(arg);
}

void log_error(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vlog_level(LOG_ERROR, format, arg);
    va_end(arg);
}

void log_perror(const char *s) {
    vlog_level(LOG_PERROR, s, NULL);
}

#endif // DCCS_LOGGING_H

