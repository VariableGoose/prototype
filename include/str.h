#pragma once

#include "core.h"

#include <stddef.h>
#include <string.h>

typedef struct Str Str;
struct Str {
    const u8 *data;
    u64 len;
};

#define str(data, len) ((Str) {data, len})
#define cstr(str) ((Str) {str, strlen(str)})
#define str_lit(str) ((Str) {(const u8 *) (str), sizeof(str)-1})
#define str_arg(str) (int) (str).len, (str).data

extern size_t str_hash(const void *str, size_t size);
extern int str_cmp(const void *a, const void *b, size_t size);

extern Str str_copy(Str str, Allocator allocator);

// Allocates a char buffer that needs to be free with the same allocator
// provided.
extern Str read_file(const char *filepath, Allocator allocator);
