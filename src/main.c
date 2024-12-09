#include "ecs.h"
#include "gfx.h"
#include "core.h"
#include "linear_algebra.h"
#include "window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <stdio.h>

typedef struct GameState GameState;
struct GameState {
    ECS *ecs;
    Window *window;
    Renderer *renderer;
    f32 dt;
    f32 dt_fixed;

    SystemGroup free_group;
    SystemGroup fixed_group;
};

// -- Components ---------------------------------------------------------------

typedef struct Transform Transform;
struct Transform {
    Vec2 pos;
    Vec2 size;
};

typedef struct PlayerController PlayerController;
struct PlayerController {
    f32 speed;
};

typedef struct Renderable Renderable;
struct Renderable {
    Color color;
    Texture texture;
};

// -----------------------------------------------------------------------------

// -- Systems ------------------------------------------------------------------

void player_control_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    PlayerController *controller = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        Vec2 velocity = vec2s(0.0f);
        velocity.x -= key_down(state->window, KEY_A);
        velocity.x += key_down(state->window, KEY_D);
        velocity.y -= key_down(state->window, KEY_S);
        velocity.y += key_down(state->window, KEY_W);
        velocity = vec2_normalized(velocity);
        velocity = vec2_muls(velocity, controller[i].speed*state->dt);

        transform[i].pos = vec2_add(transform[i].pos, velocity);
    }
}

void render_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    Renderable *renderable = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        renderer_draw_quad(state->renderer, (Quad) {
                .pos = transform[i].pos,
                .size = transform[i].size,
            }, vec2s(0.0f), renderable[i].texture, renderable[i].color);
    }
}

// -----------------------------------------------------------------------------

GameState game_state_new(void) {
    Window *window = window_new(1280, 720, "Prototype", false, ALLOCATOR_LIBC);
    gfx_init(glfwGetProcAddress);
    return (GameState) {
        .ecs = ecs_new(),
        .window = window,
        .renderer = renderer_new(4096, ALLOCATOR_LIBC),
        .dt_fixed = 1.0f / 10.0f,
    };
}

void game_state_free(GameState *state) {
    ecs_free(state->ecs);
    // Always call 'renderer_free()' before 'window_free()' becaues the former
    // uses OpenGL functions that are no longer available after 'window_free()'
    // has been called.
    renderer_free(state->renderer);
    window_free(state->window);
}

void setup_ecs(GameState *state) {
    state->free_group = ecs_system_group(state->ecs);
    state->fixed_group = ecs_system_group(state->ecs);

    ecs_register_component(state->ecs, Transform);
    ecs_register_component(state->ecs, PlayerController);
    ecs_register_component(state->ecs, Renderable);

    ecs_register_system(state->ecs, player_control_system, state->free_group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Transform),
                [1] = ecs_id(state->ecs, PlayerController),
            },
        });
}

i32 main(void) {
    GameState game_state = game_state_new();
    setup_ecs(&game_state);

    Entity player = ecs_entity(game_state.ecs);
    entity_add_component(game_state.ecs, player, Transform, {
            .pos = vec2s(0.0f),
            .size = vec2s(1.0f),
        });
    entity_add_component(game_state.ecs, player, PlayerController, {
            .speed = 25.0f,
        });
    entity_add_component(game_state.ecs, player, Renderable, {
            .color = COLOR_RED,
            .texture = TEXTURE_NULL,
        });

    f32 last_time_step = 0.0f;
    f32 last = glfwGetTime();
    while (window_is_open(game_state.window)) {
        f32 curr = glfwGetTime();
        game_state.dt = curr - last;
        last = curr;

        // Game logic
        last_time_step += game_state.dt;
        for (u8 i = 0;
                i < 10 && last_time_step >= game_state.dt_fixed;
                i++, last_time_step -= game_state.dt_fixed) {
            ecs_run_group(game_state.ecs, game_state.fixed_group);
        }
        ecs_run_group(game_state.ecs, game_state.free_group);

        // Rendering
        glClearColor(color_arg(COLOR_BLACK));
        glClear(GL_COLOR_BUFFER_BIT);
        renderer_begin(game_state.renderer, (Camera) {
                .screen_size = window_get_size(game_state.window),
                .direction = vec2s(1.0f),
                .position = vec2s(0.0f),
                .zoom = 25.0f,
            });

        ecs_run_system(game_state.ecs, render_system, (QueryDesc) {
                .user_ptr = &game_state,
                .fields = {
                    [0] = ecs_id(game_state.ecs, Transform),
                    [1] = ecs_id(game_state.ecs, Renderable),
                },
            });

        renderer_end(game_state.renderer);
        renderer_submit(game_state.renderer);

        window_poll_event(game_state.window);
        window_swap_buffers(game_state.window);
    }

    game_state_free(&game_state);
    return 0;
}
