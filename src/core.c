#include "core.h"

#include <stdlib.h>

void *malloc_stub(size_t size, void *ctx) {
    (void) ctx;
    return malloc(size);
}

void free_stub(void *ptr, size_t size, void *ctx) {
    (void) ctx;
    (void) size;
    return free(ptr);
}

void *realloc_stub(void *ptr, size_t old, size_t new, void *ctx) {
    (void) ctx;
    (void) old;
    return realloc(ptr, new);
}
