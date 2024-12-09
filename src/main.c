#include "ecs.h"
#include "gfx.h"
#include "core.h"
#include "linear_algebra.h"
#include "window.h"
#include "ds.h"

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
    f32 gravity;

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

typedef struct PhysicsBody PhysicsBody;
struct PhysicsBody {
    f32 gravity_multiplier;
    Vec2 velocity;
    b8 is_static;
};

// -----------------------------------------------------------------------------

// -- Systems ------------------------------------------------------------------

void template_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    for (u32 i = 0; i < iter.count; i++) {
    }
}

void player_control_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    PlayerController *controller = ecs_query_iter_get_field(iter, 0);
    PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        body[i].velocity.x = 0.0f;
        body[i].velocity.x -= key_down(state->window, KEY_A);
        body[i].velocity.x += key_down(state->window, KEY_D);
        body[i].velocity.x *= controller[i].speed;

        if (key_press(state->window, KEY_SPACE)) {
            body[i].velocity.y = 30.0f;
        }

        if (key_release(state->window, KEY_SPACE) && body[i].velocity.y > 0.0f) {
            body[i].velocity.y /= 2.0f;
        }
    }
}

void physics_step_body(GameState *state, Transform *transform, PhysicsBody *body, f32 dt) {
    if (body->is_static) {
        return;
    }
    body->velocity.y += state->gravity*body->gravity_multiplier*state->dt_fixed;
    transform->pos = vec2_add(transform->pos, vec2_muls(body->velocity, dt));
}

void physics_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        physics_step_body(state, &transform[i], &body[i], state->dt_fixed);
    }

    // TODO: Implement spatial hashing and do collision detection/resolution
    // with swept aabb.
}

// -----------------------------------------------------------------------------

GameState game_state_new(void) {
    Window *window = window_new(1280, 720, "Prototype", false, ALLOCATOR_LIBC);
    gfx_init(glfwGetProcAddress);
    return (GameState) {
        .ecs = ecs_new(),
        .window = window,
        .renderer = renderer_new(4096, ALLOCATOR_LIBC),
        .dt_fixed = 1.0f / 50.0f,
        .gravity = -9.82f,
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
    ecs_register_component(state->ecs, PhysicsBody);

    ecs_register_system(state->ecs, player_control_system, state->free_group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, PlayerController),
                [1] = ecs_id(state->ecs, PhysicsBody),
                QUERY_FIELDS_END,
            },
        });

    ecs_register_system(state->ecs, physics_system, state->fixed_group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Transform),
                [1] = ecs_id(state->ecs, PhysicsBody),
                QUERY_FIELDS_END,
            },
        });
}

typedef struct PhysicsObject PhysicsObject;
struct PhysicsObject {
    Transform transform;
    Renderable renderable;
    PhysicsBody body;
};

i32 main(void) {
    GameState game_state = game_state_new();
    setup_ecs(&game_state);
    window_set_vsync(game_state.window, false);

    Entity player = ecs_entity(game_state.ecs);
    entity_add_component(game_state.ecs, player, Transform, {
            .pos = vec2s(0.0f),
            .size = vec2s(1.0f),
        });
    entity_add_component(game_state.ecs, player, PlayerController, {
            .speed = 25.0f,
        });
    entity_add_component(game_state.ecs, player, Renderable, {
            .color = COLOR_WHITE,
            .texture = TEXTURE_NULL,
        });
    entity_add_component(game_state.ecs, player, PhysicsBody, {
            .gravity_multiplier = 10.0f,
            .is_static = false,
        });

    Entity platform = ecs_entity(game_state.ecs);
    entity_add_component(game_state.ecs, platform, Transform, {
            .pos = vec2(0.0f, -10.0f),
            .size = vec2(10.0f, 1.0f),
        });
    entity_add_component(game_state.ecs, platform, Renderable, {
            .color = color_rgb_hex(0x212121),
            .texture = TEXTURE_NULL,
        });
    entity_add_component(game_state.ecs, platform, PhysicsBody, {
            .is_static = true,
        });

    u32 fps = 0;
    f32 fps_timer = 0.0f;

    Vec(PhysicsObject) snapshot = NULL;

    f32 last_time_step = 0.0f;
    f32 last = glfwGetTime();
    while (window_is_open(game_state.window)) {
        f32 curr = glfwGetTime();
        game_state.dt = curr - last;
        last = curr;

        fps++;
        fps_timer += game_state.dt;
        if (fps_timer >= 1.0f) {
            printf("FPS: %u\n", fps);
            fps = 0;
            fps_timer = 0.0f;
        }

        // Game logic
        last_time_step += game_state.dt;
        for (u8 i = 0;
                i < 10 && last_time_step >= game_state.dt_fixed;
                i++, last_time_step -= game_state.dt_fixed) {
            vec_free(snapshot);
            Query query = ecs_query(game_state.ecs, (QueryDesc) {
                    .fields = {
                        ecs_id(game_state.ecs, Transform),
                        ecs_id(game_state.ecs, Renderable),
                        ecs_id(game_state.ecs, PhysicsBody),
                    },
                });
            for (u32 i = 0; i < query.count; i++) {
                QueryIter iter = ecs_query_get_iter(query, i);
                Transform *transform = ecs_query_iter_get_field(iter, 0);
                Renderable *renderable = ecs_query_iter_get_field(iter, 1);
                PhysicsBody *body = ecs_query_iter_get_field(iter, 2);
                for (u32 j = 0; j < iter.count; j++) {
                    vec_push(snapshot, ((PhysicsObject) {
                            .transform = transform[j],
                            .renderable = renderable[j],
                            .body = body[j],
                        }));
                }
            }

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

        for (u32 i = 0; i < vec_len(snapshot); i++) {
            PhysicsObject obj = snapshot[i];
            physics_step_body(&game_state, &obj.transform, &obj.body, last_time_step);
            renderer_draw_quad(game_state.renderer, (Quad) {
                    .pos = obj.transform.pos,
                    .size = obj.transform.size,
                }, vec2s(0.0f), obj.renderable.texture, obj.renderable.color);
        }

        renderer_end(game_state.renderer);
        renderer_submit(game_state.renderer);

        window_poll_event(game_state.window);
        window_swap_buffers(game_state.window);
    }

    game_state_free(&game_state);
    return 0;
}
