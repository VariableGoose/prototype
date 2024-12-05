#include "core.h"

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ecs.h"
#include "gfx.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

typedef Quad Transform;

typedef struct Renderable Renderable;
struct Renderable {
    Color color;
    Texture texture;
};

f32 get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/1e9;
}

void resize_cb(GLFWwindow* window, i32 width, i32 height) {
    (void) window;
    glViewport(0, 0, width, height);
}

void render(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    Renderer *renderer = user_ptr;

    Renderable *r = ecs_query_iter_get_field(iter, 0);
    Transform *t = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        renderer_draw_quad(renderer, t[i], r[i].texture, r[i].color);
    }
}

i32 main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Transform);
    ecs_register_component(ecs, Renderable);

    offsetof(Transform, x);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Frame engine", NULL, NULL);
    glfwSetFramebufferSizeCallback(window, resize_cb);
    // glfwSwapInterval(0);
    glfwMakeContextCurrent(window);

    gfx_init(glfwGetProcAddress);

    Renderer *renderer = renderer_new(4096, ALLOCATOR_LIBC);

    u32 size = 64;
    Font *font = font_init(str_lit("assets/fonts/Spline_Sans/static/SplineSans-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);
    // Font font = font_init(str_lit("assets/fonts/Roboto/Roboto-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);
    // Font font = font_init(str_lit("assets/fonts/soulside/SoulsideBetrayed-3lazX.ttf"), renderer, false, ALLOCATOR_LIBC);
    // Font font = font_init(str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(color_arg(color_rgb_hex(0x000000)));
        glClear(GL_COLOR_BUFFER_BIT);

        i32 width, height;
        glfwGetWindowSize(window, &width, &height);
        renderer_update(renderer, width, height);
        renderer_begin(renderer);

        {
            Ivec2 pos = ivec2s(16);
            renderer_draw_string(renderer, str_lit("Big Red"), font, 64, pos, COLOR_RED);
            pos.y += font_get_metrics(font, 64).line_gap;
            renderer_draw_string(renderer, str_lit("Medium Blue"), font, 32, pos, COLOR_BLUE);
            pos.y += font_get_metrics(font, 32).line_gap;
            renderer_draw_string(renderer, str_lit("Small green"), font, 16, pos, COLOR_GREEN);
        }

        // const Str string = str_lit("We live on a placid island of ignorance amidst black seas of infinity, and it was not meant we should voyage far.");
        const Str string = str_lit("Femboys :3");
        Ivec2 string_size = font_measure_string(font, string, size);
        FontMetrics metrics = font_get_metrics(font, size);

        Ivec2 pos = {
            .x = (width - string_size.x) / 2,
            .y = (height - string_size.y) / 2,
        };

        // Ivec2 padding = { 8, 8 };
        // renderer_draw_quad(renderer, (Quad) {
        //         .x = pos.x - padding.x, .y = pos.y - padding.y,
        //         .w = string_size.x + padding.x * 2, .h = string_size.y + padding.y * 2,
        //     }, TEXTURE_NULL, color_rgb_hex(0x253a5e));

        // renderer_draw_quad(renderer, (Quad) {
        //         .x = pos.x - padding.x/2, .y = pos.y + metrics.ascent,
        //         .w = string_size.x + padding.x, .h = 1,
        //     }, TEXTURE_NULL, color_rgb_hex(0x253a5e));

        f32 hue = 180.0f*glfwGetTime();
        for (u64 i = 0; i < string.len; i++) {
            Glyph glyph = font_get_glyph(font, string.data[i], size);
            f32 y_offset = sinf(rad(hue-20*i)/5)*10.0f;
            renderer_draw_quad_atlas(renderer, (Quad) {
                    .x = pos.x + glyph.offset.x,
                    .y = pos.y - glyph.offset.y + metrics.ascent + y_offset,
                    .w = glyph.size.x,
                    .h = glyph.size.y,
                }, (TextureAtlas) {
                    .atlas = font_get_atlas(font, size),
                    .u0 = glyph.uv[0].x,
                    .v0 = glyph.uv[0].y,
                    .u1 = glyph.uv[1].x,
                    .v1 = glyph.uv[1].y,
                }, color_hsv(hue - 15*i, 0.85f, 1.0f));
            pos.x += glyph.advance;
        }

        // ecs_run_system(ecs, render, (QueryDesc) {
        //         .user_ptr = renderer, 
        //         .fields = {
        //             [0] = ecs_id(ecs, Renderable),
        //             [1] = ecs_id(ecs, Transform),
        //         },
        //     });

        renderer_end(renderer);
        renderer_submit(renderer);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    font_free(font);

    renderer_free(renderer);

    glfwDestroyWindow(window);
    glfwTerminate();

    ecs_free(ecs);
    return 0;
}
