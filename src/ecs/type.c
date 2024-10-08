#include "ds.h"
#include "internal.h"

#include <assert.h>
#include <stdio.h>

void type_add(Type type, ComponentId component) {
    for (size_t i = 0; i < vec_len(type); i++) {
        if (type[i] == component) {
            return;
        } else if (type[i] > component) {
            vec_insert(type, i, component);
            return;
        }
    }

    vec_push(type, component);
}

void type_remove(Type type, ComponentId component) {
    for (size_t i = 0; i < vec_len(type); i++) {
        if (type[i] == component) {
            vec_remove(type, i);
            return;
        }
    }
}

Type type_clone(const Type type) {
    Type clone = NULL;
    vec_insert_arr(clone, 0, type, vec_len(type));
    return clone;
}

size_t type_hash(const void *data, size_t size) {
    (void) size;
    Type const *type = data;
    return fvn1a_hash(*type, vec_len(*type)*sizeof(ComponentId));
}

int type_cmp(const void *a, const void *b, size_t size) {
    (void) size;
    Type const *_a = a;
    Type const *_b = b;
    return memcmp(*_a, *_b, vec_len(*_a) * sizeof(ComponentId));
}

void type_free(Type type) {
    vec_free(type);
}

size_t type_len(Type type) {
    return vec_len(type);
}

bool type_is_proper_subset(const Type a, const Type b, ComponentId *diff_component) {
    if (type_len(a) >= type_len(b)) {
        return false;
    }

    size_t i = 0;
    size_t j = 0;
    while (i < type_len(a)) {
        if (a[i] > b[j]) {
            j++;
        } else if (a[i] < b[j]) {
            i++;
        } else {
            i++;
            j++;
        }
    }

    if (diff_component != NULL) {
        *diff_component = b[j];
    }

    return i == vec_len(a);
}

void type_inspect(const Type type) {
    for (size_t i = 0; i < type_len(type); i++) {
        printf("%zu", type[i]);
        if (i < type_len(type) - 1) {
            printf(", ");
        }
    }
    printf("\n");
}
