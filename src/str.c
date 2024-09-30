#include "str.h"
#include "ds.h"

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
