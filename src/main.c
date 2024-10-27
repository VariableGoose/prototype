#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include "ecs.h"
#include "gfx.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

typedef Quad Transform;

typedef struct Renderable Renderable;
struct Renderable {
    Color color;
};

float get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/1e9;
}

void resize_cb(GLFWwindow* window, int width, int height) {
    (void) window;
    glViewport(0, 0, width, height);
}

void render(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    Renderer *renderer = user_ptr;

    Renderable *r = ecs_query_iter_get_field(iter, 0);
    Transform *t = ecs_query_iter_get_field(iter, 1);
    for (size_t i = 0; i < iter.count; i++) {
        renderer_draw_quad(renderer, t[i], r[i].color);
    }
}

int main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Transform);
    ecs_register_component(ecs, Renderable);

    Entity ent = ecs_entity(ecs);
    entity_add_component(ecs, ent, Transform, {100, 100, 100, 100});
    entity_add_component(ecs, ent, Renderable, {color_rgb_hex(0xff00ff)});

    Entity ent2 = ecs_entity(ecs);
    entity_add_component(ecs, ent2, Transform, {100, 300, 100, 50});
    entity_add_component(ecs, ent2, Renderable, {color_rgb_hex(0xffff00)});

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Frame engine", NULL, NULL);
    glfwSetFramebufferSizeCallback(window, resize_cb);
    // glfwSwapInterval(0);
    glfwMakeContextCurrent(window);

    // TODO: Move the glad dependency into the renderer so the user doesn't
    // have to worry about it.
    if (!gladLoadGL(glfwGetProcAddress)) {
        printf("ERROR: GLAD failed to load.\n");
        return 1;
    }

    Renderer *renderer = renderer_new(4096);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(color_arg(color_rgb_hex(0x000000)));
        glClear(GL_COLOR_BUFFER_BIT);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        renderer_update(renderer, width, height);
        renderer_begin(renderer);

        ecs_run_system(ecs, render, (QueryDesc) {
                .user_ptr = renderer, 
                .fields = {
                    [0] = ecs_id(ecs, Renderable),
                    [1] = ecs_id(ecs, Transform),
                },
            });

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
