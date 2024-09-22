#include <stdio.h>

#include "ds.h"

int main(void) {
    printf("Hello, world\n");

    Vec(int) vec = NULL;

    vec_push(vec, 1);
    vec_push(vec, 42);
    vec_push(vec, 83);
    for (size_t i = 0; i < vec_len(vec); i++) {
        printf("%d\n", vec[i]);
    }

    vec_free(vec);

    return 0;
}
