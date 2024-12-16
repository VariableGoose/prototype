#include "core.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

void *malloc_stub(u64 size, void *ctx) {
    (void) ctx;
    return malloc(size);
}

void free_stub(void *ptr, u64 size, void *ctx) {
    (void) ctx;
    (void) size;
    return free(ptr);
}

void *realloc_stub(void *ptr, u64 old, u64 new, void *ctx) {
    (void) ctx;
    (void) old;
    return realloc(ptr, new);
}

void _log(LogLevel level, const char *file, u32 line, const char *fmt, ...) {
    const char *level_prefix[] = {
        "\033[0;101m[FATAL]",
        "\033[0;91m[ERROR]",
        "\033[0;93m[WARN] ",
        "\033[0;92m[INFO] ",
        "\033[0;94m[DEBUG]",
        "\033[0;95m[TRACE]"
    };

    char msg[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, arrlen(msg), fmt, args);
    va_end(args);

    printf("%s %s:%u: %s\033[0;0m\n", level_prefix[level], file, line, msg);
}
