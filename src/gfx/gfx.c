#include "gfx.h"

#include <glad/gl.h>
#include <stdio.h>

void gfx_init(GlLoadFunc gl_load_func) {
    if (!gladLoadGL(gl_load_func)) {
        printf("ERROR: GLAD failed to load.\n");
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
