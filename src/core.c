#include "core.h"

#include <stdlib.h>

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
