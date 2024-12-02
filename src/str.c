#include "str.h"
#include "ds.h"

#include <stdio.h>
#include <stdlib.h>

size_t str_hash(const void *str, size_t size) {
    (void) size;
    const Str *_str = str;
    return fvn1a_hash(_str->data, _str->len);
}

int str_cmp(const void *a, const void *b, size_t size) {
    (void) size;
    const Str *_a = a;
    const Str *_b = b;
    return memcmp(_a->data, _b->data, min(_a->len, _b->len));
}

Str str_copy(Str str, Allocator allocator) {
    Str copy = {
        .data = allocator.alloc(str.len, allocator.ctx),
        .len = str.len,
    };
    memcpy((char *) copy.data, str.data, str.len);
    return copy;
}

Str read_file(const char *filepath, Allocator allocator) {
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        printf("ERROR: Failed to open file %s.\n", filepath);
        return (Str) {0};
    }

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    u8 *buffer = allocator.alloc(len, allocator.ctx);
    fread(buffer, sizeof(char), len, fp);

    return str(buffer, len);
}
