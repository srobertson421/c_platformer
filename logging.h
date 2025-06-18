#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

// Global log file handle
extern FILE* g_logFile;

// Log levels
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// Initialize logging system
void log_init(const char* filename);

// Close logging system
void log_close(void);

// Write log message
void log_write(LogLevel level, const char* format, ...);

// Convenience macros
#define LOG_DEBUG(...) log_write(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) log_write(LOG_INFO, __VA_ARGS__)
#define LOG_WARNING(...) log_write(LOG_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_ERROR, __VA_ARGS__)

#endif // LOGGING_H