#pragma once

#include <stddef.h>
#include <string.h>

typedef struct Str Str;
struct Str {
    const char *data;
    size_t len;
};

#define str(data, len) ((Str) {data, len})
#define cstr(str) ((Str) {str, strlen(str)})
#define str_lit(str) ((Str) {str, sizeof(str)-1})

extern size_t str_hash(const void *str, size_t size);
extern int str_cmp(const void *a, const void *b, size_t size);
