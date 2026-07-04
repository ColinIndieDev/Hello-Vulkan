#pragma once

#include <stdint.h>

typedef enum : uint8_t {
    SEVERITY_INFO = 0,
    SEVERITY_WARNING,
    SEVERITY_ERROR
} severity_t;

void log_msg(severity_t severtiy, const char *msg, ...);
