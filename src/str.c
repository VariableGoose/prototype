#include "str.h"
#include "ds.h"

#include <stdio.h>
#include <stdlib.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

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

Str read_file(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        printf("ERROR: Failed to open file %s.\n", filepath);
        return (Str) {0};
    }

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buffer = malloc(len);
    fread(buffer, sizeof(char), len, fp);

    return str(buffer, len);
}
