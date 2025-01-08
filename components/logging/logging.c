#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "logging.h"


#define LOG_COLOR_RESET   "\033[0m"
#define LOG_COLOR_ERROR   "\033[31m" // Red
#define LOG_COLOR_WARN    "\033[33m" // Yellow
#define LOG_COLOR_INFO    "\033[32m" // Green
#define LOG_COLOR_DEBUG   "\033[34m" // Blue
#define LOG_COLOR_VERBOSE "\033[37m" // White


static log_level_t global_log_level = LOG_LEVEL_DEBUG;


static const char* log_level_color(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_ERROR:   return LOG_COLOR_ERROR;
        case LOG_LEVEL_WARN:    return LOG_COLOR_WARN;
        case LOG_LEVEL_INFO:    return LOG_COLOR_INFO;
        case LOG_LEVEL_DEBUG:   return LOG_COLOR_DEBUG;
        case LOG_LEVEL_VERBOSE: return LOG_COLOR_VERBOSE;
        default:                return LOG_COLOR_RESET;
    }
}


static const char log_level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_ERROR:   return 'E';
        case LOG_LEVEL_WARN:    return 'W';
        case LOG_LEVEL_INFO:    return 'I';
        case LOG_LEVEL_DEBUG:   return 'D';
        case LOG_LEVEL_VERBOSE: return 'V';
        default:                return '-';
    }
}


void log_message(log_level_t level, const char* tag, const char* format, ...) {
    if (level > global_log_level) {
        return;
    }
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_buffer[20];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", t);
    printf("%s%c (%s) %s: ", log_level_color(level), log_level_to_string(level), time_buffer, tag);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("%s\n", LOG_COLOR_RESET);
}


void logging_set_global_log_level(log_level_t level) {
    global_log_level = level;
}
