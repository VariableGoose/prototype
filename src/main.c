#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ecs.h"
#include "gfx.h"
#include "core.h"
#include "window.h"

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

void render(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    Renderer *renderer = user_ptr;

    Renderable *r = ecs_query_iter_get_field(iter, 0);
    Transform *t = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        renderer_draw_quad(renderer, t[i], vec2s(0.0f), r[i].texture, r[i].color);
    }
}

i32 main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Transform);
    ecs_register_component(ecs, Renderable);

    Window *window = window_new(800, 600, "Frame Engine", false, ALLOCATOR_LIBC);

    gfx_init(glfwGetProcAddress);

    Renderer *renderer = renderer_new(4096, ALLOCATOR_LIBC);

    Font *font = font_init(str_lit("assets/fonts/Spline_Sans/static/SplineSans-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);
    // Font font = font_init(str_lit("assets/fonts/Roboto/Roboto-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);
    // Font font = font_init(str_lit("assets/fonts/soulside/SoulsideBetrayed-3lazX.ttf"), renderer, false, ALLOCATOR_LIBC);
    // Font font = font_init(str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), renderer, false, ALLOCATOR_LIBC);

    Vec2 player_pos = vec2s(0.0f);
    Vec2 camera_pos = vec2s(0.0f);

    f32 last = glfwGetTime();
    while (window_is_open(window)) {
        f32 curr = glfwGetTime();
        f32 dt = curr - last;
        last = curr;

        glClearColor(color_arg(color_rgb_hex(0x000000)));
        glClear(GL_COLOR_BUFFER_BIT);

        Ivec2 screen_size = window_get_size(window);

        // Game pass
        Vec2 vel = vec2s(0.0f);
        vel.x -= key_down(window, KEY_A);
        vel.x += key_down(window, KEY_D);
        vel.y -= key_down(window, KEY_S);
        vel.y += key_down(window, KEY_W);
        vel = vec2_normalized(vel);
        player_pos = vec2_add(player_pos, vec2_muls(vel, dt*25.0f));
        camera_pos = vec2_lerp(camera_pos, player_pos, dt*10.0f);

        renderer_begin(renderer, (Camera) {
                .screen_size = screen_size,
                .zoom = 15.0f,
                .position = camera_pos,
                .direction = vec2(1.0f, 1.0f),
            });

        renderer_draw_quad(renderer, (Quad) {
                .pos = vec2(2.0f, 1.0f),
                .size = vec2(1.0f, 1.0f),
            }, vec2(0.0f, 0.0f), TEXTURE_NULL, COLOR_GREEN);

        renderer_draw_quad(renderer, (Quad) {
                .pos = player_pos,
                .size = vec2(1.0f, 1.0f),
            }, vec2(0.0f, 0.0f), TEXTURE_NULL, COLOR_WHITE);

        // ecs_run_system(ecs, render, (QueryDesc) {
        //         .user_ptr = renderer, 
        //         .fields = {
        //             [0] = ecs_id(ecs, Renderable),
        //             [1] = ecs_id(ecs, Transform),
        //         },
        //     });

        renderer_end(renderer);
        renderer_submit(renderer);

        // UI pass
        renderer_begin(renderer, (Camera) {
                .screen_size = screen_size,
                .zoom = screen_size.y,
                .position = vec2(vec2_arg(ivec2_divs(screen_size, 2))),
                .direction = vec2(1.0f, -1.0f),
            });

        Ivec2 pos = ivec2s(16);
        renderer_draw_string(renderer, str_lit("Big Red"), font, 64, pos, COLOR_RED);
        pos.y += font_get_metrics(font, 64).line_gap;
        renderer_draw_string(renderer, str_lit("Medium Blue"), font, 32, pos, COLOR_BLUE);
        pos.y += font_get_metrics(font, 32).line_gap;
        renderer_draw_string(renderer, str_lit("Small green"), font, 16, pos, COLOR_GREEN);

        renderer_end(renderer);
        renderer_submit(renderer);

        window_swap_buffers(window);
        window_poll_event(window);
    }

    font_free(font);

    renderer_free(renderer);

    window_free(window);

    ecs_free(ecs);
    return 0;
}
