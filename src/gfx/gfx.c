#include "gfx.h"

#include <glad/gl.h>
#include <stdio.h>

void gfx_init(GlLoadFunc gl_load_func) {
    // TODO: Move the glad dependency into the renderer so the user doesn't
    // have to worry about it.
    if (!gladLoadGL(gl_load_func)) {
        printf("ERROR: GLAD failed to load.\n");
    }
}
