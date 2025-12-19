#pragma once
#include <stdio.h>
#include <stdarg.h>

using namespace std;

// Call this at the start of main() to ensure logs appear in Docker
inline void init_logging() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

inline void log(const char *format, ...) {
    va_list   args;
    char      buffer[255];

    va_start(args, format);
    time_t current_time = time(nullptr);
    tm *local_time = std::localtime(&current_time);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
    printf("%s: ", buffer);
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    va_end(args);
}
