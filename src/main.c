#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include "ecs.h"
#include "gfx.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

float get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/1e9;
}

int main(void) {
    ECS *ecs = ecs_new();

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Frame engine", NULL, NULL);
    glfwSwapInterval(0);
    glfwMakeContextCurrent(window);

    // TODO: Move the glad dependency into the renderer so the user doesn't
    // have to worry about it.
    if (!gladLoadGL(glfwGetProcAddress)) {
        printf("ERROR: GLAD failed to load.\n");
        return 1;
    }

    Renderer *renderer = renderer_new(4096);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(color_arg(color_rgb_hex(0x212121)));
        glClear(GL_COLOR_BUFFER_BIT);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        renderer_update(renderer, width, height);
        renderer_begin(renderer);
        renderer_draw_quad(renderer, (Quad) {
                .x = 100, .y = 100,
                .w = 100, .h = 100,
            }, color_rgb_hex(0xffffff));

        double x, y;
        glfwGetCursorPos(window, &x, &y);
        renderer_draw_quad(renderer, (Quad) {
                .x = x, .y = y,
                .w = 100, .h = 150,
            }, color_rgb_hex(0xffffff));

        renderer_end(renderer);
        renderer_submit(renderer);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer_free(renderer);

    glfwDestroyWindow(window);
    glfwTerminate();

    ecs_free(ecs);
    return 0;
}
