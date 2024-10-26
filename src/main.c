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

    if (!gladLoadGL(glfwGetProcAddress)) {
        printf("ERROR: GLAD failed to load.\n");
        return 1;
    }

    Renderer *renderer = renderer_new(4096);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(color_arg(color_rgb_hex(0x212121)));
        glClear(GL_COLOR_BUFFER_BIT);

        renderer_begin(renderer);
        renderer_draw_quad(renderer, (Quad) {
                .x = 0, .y = 0,
                .w = 1, .h = 1,
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
