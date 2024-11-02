#include "gfx.h"

#include <glad/gl.h>
#include <stdio.h>

void gfx_init(GlLoadFunc gl_load_func) {
    if (!gladLoadGL(gl_load_func)) {
        printf("ERROR: GLAD failed to load.\n");
    }
}
