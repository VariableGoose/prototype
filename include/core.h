#pragma once

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char      i8;
typedef signed short     i16;
typedef signed int       i32;
typedef signed long long i64;

typedef float f32;
typedef double f64;

typedef u8 b8;
typedef u32 b32;

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(S, M) ((u64) (&((S *) 0)->M))
#define arrlen(ARR) (sizeof(ARR)/(sizeof((ARR)[0])))

#define clamp(V, A, B) ((V) > (A) ? (A) : (V) < (B) ? (B) : (V))
#define min(A, B) ((A) < (B) ? (A) : (B))
#define max(A, B) ((A) > (B) ? (A) : (B))
#define lerp(A, B, T) ((A) + ((B) - (A)) * (T))

typedef struct Allocator Allocator;
struct Allocator {
    void *(*alloc)(u64 size, void *ctx);
    void (*free)(void *ptr, u64 size, void *ctx);
    void *(*realloc)(void *ptr, u64 old, u64 new, void *ctx);
    void *ctx;
};

extern void *malloc_stub(u64 size, void *ctx);
extern void free_stub(void *ptr, u64 size, void *ctx);
extern void *realloc_stub(void *ptr, u64 old, u64 new, void *ctx);

static const Allocator ALLOCATOR_LIBC = {
    .alloc = malloc_stub,
    .free = free_stub,
    .realloc = realloc_stub,
    .ctx = NULL,
};
