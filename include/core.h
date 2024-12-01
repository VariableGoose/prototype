#pragma once

#include <stddef.h>

typedef struct Allocator Allocator;
struct Allocator {
    void *(*alloc)(size_t size, void *ctx);
    void (*free)(void *ptr, size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old, size_t new, void *ctx);
    void *ctx;
};

extern void *malloc_stub(size_t size, void *ctx);
extern void free_stub(void *ptr, size_t size, void *ctx);
extern void *realloc_stub(void *ptr, size_t old, size_t new, void *ctx);

static const Allocator ALLOCATOR_LIBC = {
    .alloc = malloc_stub,
    .free = free_stub,
    .realloc = realloc_stub,
    .ctx = NULL,
};
