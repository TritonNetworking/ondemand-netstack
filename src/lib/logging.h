/**
 * Utility functions.
 */

#ifndef DCCS_LOGGING_H
#define DCCS_LOGGING_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    switch (level) {
        case LOG_VERBOSE:
            fprintf(stderr, "[%sverbose%s] ", KBLU, KNRM);
            vfprintf(stderr, format, arg);
            break;
        case LOG_INFO:
            fprintf(stdout, "[%sinfo%s]  ", KCYN, KNRM);
            vfprintf(stdout, format, arg);
            break;
        case LOG_DEBUG:
#if DEBUG
            fprintf(stderr, "[%sdebug%s] ", KMAG, KNRM);
            vfprintf(stderr, format, arg);
#endif
            break;
        case LOG_WARNING:
            fprintf(stderr, "[%swarn%s]  ", KYEL, KNRM);
            vfprintf(stderr, format, arg);
            break;
        case LOG_ERROR:
            fprintf(stderr, "[%serror%s] ", KRED, KNRM);
            vfprintf(stderr, format, arg);
            break;
        case LOG_PERROR:
            fprintf(stderr, "[%serror%s] ", KRED, KNRM);
            perror(format);
            break;
        default:
            fprintf(stderr, "[%s%d%s]    ", KGRN, level, KNRM);
            vfprintf(stderr, format, arg);
            break;
    }
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

