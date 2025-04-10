#pragma once
#include <stdio.h>
#include <stdarg.h>

using namespace std;

void log(const char *format, ...) {
    va_list   args;
    char      buffer[255];

    va_start(args, format);
    time_t current_time = time(nullptr);
    tm *local_time = std::localtime(&current_time);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
    printf("%s: ", buffer);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}
