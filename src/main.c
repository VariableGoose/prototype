#include "core.h"

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ecs.h"
#include "gfx.h"
#include "font.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <stb_image_write.h>

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

    u32 size = 32;
    Font font = font_init(str_lit("assets/fonts/Spline_Sans/static/SplineSans-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);
    Font font2 = font_init(str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(color_arg(color_rgb_hex(0x000000)));
        glClear(GL_COLOR_BUFFER_BIT);

        i32 width, height;
        glfwGetWindowSize(window, &width, &height);
        renderer_update(renderer, width, height);
        renderer_begin(renderer);

        // Texture atlas = font_get_atlas(&font, 32);
        // renderer_draw_quad(renderer, (Quad) {
        //         // .x = texture_get_size(renderer, atlas2).x, .y = 0,
        //         .x = 0, .y = 0,
        //         .w = texture_get_size(renderer, atlas).x,
        //         .h = texture_get_size(renderer, atlas).y,
        //     }, atlas, color_rgb_hex(0xffffff));
        //
        // Texture atlas2 = font_get_atlas(&font, 16);
        // renderer_draw_quad(renderer, (Quad) {
        //         .x = texture_get_size(renderer, atlas).x, .y = 0,
        //         // .x = 0, .y = 0,
        //         .w = texture_get_size(renderer, atlas2).x,
        //         .h = texture_get_size(renderer, atlas2).y,
        //     }, atlas2, color_rgb_hex(0xffffff));

        const Str string = str_lit("Hello, World!\nThe quick fox jumps over the lazy dog!");
        FontMetrics metrics = font_get_metrics(&font, size);
        Ivec2 pos = {
            .y = metrics.ascent+metrics.descent,
        };
        pos = ivec2_add(pos, ivec2s(16));
        for (u64 i = 0; i < string.len; i++) {
            if (string.data[i] == '\n') {
                size /= 2;
                pos.x = 16;
                pos.y += metrics.line_gap;
                metrics = font_get_metrics(&font, size);
                continue;
            }

            Glyph glyph = font_get_glyph(&font, string.data[i], size);
            // printf("%c: %u\n", string.data[i], glyph.advance);
            renderer_draw_quad_atlas(renderer, (Quad) {
                    .x = pos.x + glyph.offset.x,
                    .y = pos.y - glyph.offset.y,
                    .w = glyph.size.x,
                    .h = glyph.size.y,
                }, (TextureAtlas) {
                    .atlas = font_get_atlas(&font, size),
                    .u0 = glyph.uv[0].x,
                    .v0 = glyph.uv[0].y,
                    .u1 = glyph.uv[1].x,
                    .v1 = glyph.uv[1].y,
                }, color_rgb_hex(0xffffff));
            pos.x += glyph.advance;
        }
        size *= 2;

        metrics = font_get_metrics(&font2, size);
        pos.x = 16;
        pos.y += metrics.line_gap;
        for (u64 i = 0; i < string.len; i++) {
            if (string.data[i] == '\n') {
                size /= 2;
                pos.x = 16;
                metrics = font_get_metrics(&font2, size);
                pos.y += metrics.line_gap - metrics.descent;
                continue;
            }

            Glyph glyph = font_get_glyph(&font2, string.data[i], size);
            // printf("%c: %u\n", string.data[i], glyph.advance);
            renderer_draw_quad_atlas(renderer, (Quad) {
                    .x = pos.x + glyph.offset.x,
                    .y = pos.y - glyph.offset.y,
                    .w = glyph.size.x,
                    .h = glyph.size.y,
                }, (TextureAtlas) {
                    .atlas = font_get_atlas(&font2, size),
                    .u0 = glyph.uv[0].x,
                    .v0 = glyph.uv[0].y,
                    .u1 = glyph.uv[1].x,
                    .v1 = glyph.uv[1].y,
                }, color_rgb_hex(0xffffff));
            pos.x += glyph.advance;
        }
        size *= 2;

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

    font_free(&font);

    renderer_free(renderer);

    glfwDestroyWindow(window);
    glfwTerminate();

    ecs_free(ecs);
    return 0;
}
