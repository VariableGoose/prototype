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
        renderer_draw_quad(renderer, t[i], r[i].texture, r[i].color);
    }
}

int main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Transform);
    ecs_register_component(ecs, Renderable);

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

    Renderer *renderer = renderer_new(4096);

    Font font = font_init(str_lit("assets/fonts/Spline_Sans/static/SplineSans-Regular.ttf"), 32, false);
    // Font font = font_init(str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), 24, false);

    uint8_t *rgba = malloc(font.atlas.size.x*font.atlas.size.y*4);
    for (int32_t i = 0; i < font.atlas.size.x*font.atlas.size.y; i++) {
        rgba[i*4 + 0] = 255;
        rgba[i*4 + 1] = 255;
        rgba[i*4 + 2] = 255;
        rgba[i*4 + 3] = font.atlas.buffer[i];
    }
    // stbi_write_png("atlas.png", font.atlas.size.x, font.atlas.size.y, 4, rgba, sizeof(uint8_t)*4*font.atlas.size.x);

    Texture font_texture = texture_new(renderer, (TextureDesc) {
            .width = font.atlas.size.x,
            .height = font.atlas.size.y,
            .format = TEXTURE_FORMAT_RGBA,
            .data_type = TEXTURE_DATA_TYPE_UBYTE,
            .pixels = rgba,
            .sampler = TEXTURE_SAMPLER_NEAREST,
        });

    while (!glfwWindowShouldClose(window)) {
        glClearColor(color_arg(color_rgb_hex(0x000000)));
        glClear(GL_COLOR_BUFFER_BIT);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        renderer_update(renderer, width, height);
        renderer_begin(renderer);

        // renderer_draw_quad(renderer, (Quad) {
        //         .x = 0, .y = 0,
        //         .w = font.atlas.size.x, font.atlas.size.y,
        //     }, font_texture, color_rgb_hex(0xffffff));

        const Str string = str_lit("Hello, World!\nThe quick fox jumps over the lazy dog!");
        Ivec2 pos = {
            .y = font.ascent+font.descent,
        };
        pos = ivec2_add(pos, ivec2s(16));
        for (size_t i = 0; i < string.len; i++) {
            if (string.data[i] == '\n') {
                pos.x = 16;
                pos.y += font.line_gap;
                continue;
            }

            Glyph glyph = font_get_glyph(font, string.data[i]);
            renderer_draw_quad_atlas(renderer, (Quad) {
                    .x = pos.x + glyph.offset.x,
                    .y = pos.y - glyph.offset.y,
                    .w = glyph.size.x,
                    .h = glyph.size.y,
                }, (TextureAtlas) {
                    .atlas = font_texture,
                    .u0 = (float) glyph.uv[0].x / font.atlas.size.x,
                    .v0 = (float) glyph.uv[0].y / font.atlas.size.y,
                    .u1 = (float) glyph.uv[1].x / font.atlas.size.x,
                    .v1 = (float) glyph.uv[1].y / font.atlas.size.y,
                }, color_rgb_hex(0xffffff));
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
