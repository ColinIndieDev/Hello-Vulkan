#include "logging.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include <cpstd/arena.h>

void log_msg(severity_t severtiy, const char *msg, ...) {
    char buffer[KiB(1)];
    va_list args;
    va_start(args, msg);
    vsnprintf(buffer, 1024, msg, args);
    va_end(args);

    time_t now = time(NULL);
    struct tm *time = localtime(&now); 

    switch (severtiy) {
    case SEVERITY_INFO:
        printf("[%02d:%02d:%02d INFO]: %s", time->tm_hour, time->tm_min, time->tm_sec, buffer);
        break;
    case SEVERITY_WARNING:
        fprintf(stderr, "[%02d:%02d:%02d WARNING]: %s", time->tm_hour, time->tm_min, time->tm_sec, buffer);
        break;
    case SEVERITY_ERROR:
        fprintf(stderr, "[%02d:%02d:%02d ERROR]: %s", time->tm_hour, time->tm_min, time->tm_sec, buffer);
        break;
    }
}
