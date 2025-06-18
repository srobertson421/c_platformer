#include "logging.h"

FILE* g_logFile = NULL;

void log_init(const char* filename) {
    g_logFile = fopen(filename, "w");
    if (g_logFile == NULL) {
        fprintf(stderr, "Failed to open log file: %s\n", filename);
        return;
    }
    
    // Write header
    time_t rawtime;
    struct tm* timeinfo;
    char timestamp[80];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    fprintf(g_logFile, "=== Platformer Log Started: %s ===\n", timestamp);
    fprintf(g_logFile, "Platform: ");
    
#ifdef _WIN32
    fprintf(g_logFile, "Windows\n");
#elif __APPLE__
    fprintf(g_logFile, "macOS\n");
#elif __linux__
    fprintf(g_logFile, "Linux\n");
#else
    fprintf(g_logFile, "Unknown\n");
#endif
    
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
}

void log_close(void) {
    if (g_logFile != NULL) {
        fprintf(g_logFile, "\n=== Log Closed ===\n");
        fclose(g_logFile);
        g_logFile = NULL;
    }
}

void log_write(LogLevel level, const char* format, ...) {
    if (g_logFile == NULL) return;
    
    // Get timestamp
    time_t rawtime;
    struct tm* timeinfo;
    char timestamp[80];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    
    // Write level and timestamp
    const char* levelStr;
    switch (level) {
        case LOG_DEBUG:   levelStr = "DEBUG"; break;
        case LOG_INFO:    levelStr = "INFO "; break;
        case LOG_WARNING: levelStr = "WARN "; break;
        case LOG_ERROR:   levelStr = "ERROR"; break;
        default:          levelStr = "UNKWN"; break;
    }
    
    fprintf(g_logFile, "[%s] %s: ", timestamp, levelStr);
    
    // Write the actual message
    va_list args;
    va_start(args, format);
    vfprintf(g_logFile, format, args);
    va_end(args);
    
    fprintf(g_logFile, "\n");
    fflush(g_logFile); // Ensure it's written immediately
    
    // Also print errors to stderr
    if (level == LOG_ERROR) {
        fprintf(stderr, "[%s] ERROR: ", timestamp);
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
}