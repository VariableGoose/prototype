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
#define str_arg(str) (int) (str).len, (str).data

extern size_t str_hash(const void *str, size_t size);
extern int str_cmp(const void *a, const void *b, size_t size);

// Allocates a char buffer that needs to be freed by calling 'free(str.data);'.
extern Str read_file(const char *filepath);
