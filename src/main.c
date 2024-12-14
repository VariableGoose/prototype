#include "ecs.h"
#include "gfx.h"
#include "core.h"
#include "linear_algebra.h"
#include "window.h"
#include "ds.h"
#include <math.h>

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
        body[i].velocity.y = 0.0f;
        body[i].velocity.x -= key_down(state->window, KEY_A);
        body[i].velocity.x += key_down(state->window, KEY_D);
        body[i].velocity.y -= key_down(state->window, KEY_S);
        body[i].velocity.y += key_down(state->window, KEY_W);
        body[i].velocity.x *= controller[i].speed;
        body[i].velocity.y *= controller[i].speed;

        if (key_press(state->window, KEY_SPACE)) {
            body[i].velocity.y = 30.0f;
        }

        if (key_release(state->window, KEY_SPACE) && body[i].velocity.y > 0.0f) {
            body[i].velocity.y /= 2.0f;
        }
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
}

typedef struct PhysicsObject PhysicsObject;
struct PhysicsObject {
    Entity id;
    Transform transform;
    PhysicsBody body;
};

typedef struct PhysicsWorld PhysicsWorld;
struct PhysicsWorld {
    Vec(PhysicsObject) bodies;
};

PhysicsWorld physics_world_init(GameState *state) {
    PhysicsWorld world = {0};

    // Pull entities out of the ecs.
    Query query = ecs_query(state->ecs, (QueryDesc) {
            .fields = {
                ecs_id(state->ecs, Transform),
                ecs_id(state->ecs, PhysicsBody),
            },
        });
    for (u32 i = 0; i < query.count; i++) {
        QueryIter iter = ecs_query_get_iter(query, i);
        Transform *transform = ecs_query_iter_get_field(iter, 0);
        PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
        for (u32 j = 0; j < iter.count; j++) {
            PhysicsObject obj = {
                .id = ecs_query_iter_get_entity(iter, j),
                .transform = transform[j],
                .body = body[j],
            };
            vec_push(world.bodies, obj);
        }
    }
    ecs_query_free(query);

    return world;
}

void physics_world_step(PhysicsWorld *world, GameState *state, f32 dt) {
    for (u32 i = 0; i < vec_len(world->bodies); i++) {
        PhysicsObject *obj = &world->bodies[i];
        if (obj->body.is_static) {
            return;
        }
        obj->body.velocity.y += state->gravity*obj->body.gravity_multiplier*dt;
        obj->transform.pos = vec2_add(obj->transform.pos, vec2_muls(obj->body.velocity, dt));
    }

    for (u32 i = 0; i < vec_len(world->bodies); i++) {
        for (u32 j = 0; j < vec_len(world->bodies); j++) {
        }
    }
}

void physics_world_update_ecs(PhysicsWorld *world, GameState *state) {
    for (u32 i = 0; i < vec_len(world->bodies); i++) {
        PhysicsObject obj = world->bodies[i];
        Transform *transform = entity_get_component(state->ecs, obj.id, Transform);
        PhysicsBody *body = entity_get_component(state->ecs, obj.id, PhysicsBody);
        *transform = obj.transform;
        *body = obj.body;
    }
}

void physics_world_free(PhysicsWorld *world) {
    vec_free(world->bodies);
}

PhysicsWorld physics_world_snapshot(PhysicsWorld *world) {
    PhysicsWorld snapshot = {0};
    vec_insert_arr(snapshot.bodies, 0, world->bodies, vec_len(world->bodies));
    return snapshot;
}

typedef struct CollisionManifold CollisionManifold;
struct CollisionManifold {
    b8 is_colliding;
    Vec2 depth;
    Vec2 normal;
};

CollisionManifold colliding(Quad a, Quad b) {
    CollisionManifold manifold = {0};

    // https://blog.hamaluik.ca/posts/simple-aabb-collision-using-minkowski-difference/
    Vec2 a_bottom_left = vec2_sub(a.pos, vec2_divs(a.size, 2.0f));
    Vec2 b_top_right = vec2_add(b.pos, vec2_divs(b.size, 2.0f));
    Vec2 size = vec2_add(a.size, b.size);
    Vec2 min = vec2_sub(a_bottom_left, b_top_right);
    Vec2 max = vec2_add(min, size);
    manifold.is_colliding = (
            min.x <= 0.0f &&
            max.x >= 0.0f &&
            min.y <= 0.0f &&
            max.y >= 0.0f
        );

    if (!manifold.is_colliding) {
        return manifold;
    }

    f32 min_dist = fabsf(-min.x);
    Vec2 bound_point = vec2(min.x, 0.0f);
    if (fabsf(max.x) < min_dist) {
        min_dist = fabsf(max.x);
        bound_point = vec2(max.x, 0.0f);
    }
    if (fabsf(max.y) < min_dist) {
        min_dist = fabsf(max.y);
        bound_point = vec2(0.0f, max.y);
    }
    if (fabsf(min.y) < min_dist) {
        min_dist = fabsf(min.y);
        bound_point = vec2(0.0f, min.y);
    }
    manifold.depth = bound_point;

    if (fabsf(manifold.depth.x) > fabsf(manifold.depth.y)) {
        if (manifold.depth.x > 0.0f) {
            manifold.normal.x = 1.0f;
        } else {
            manifold.normal.x = -1.0f;
        }
    } else {
        if (manifold.depth.y > 0.0f) {
            manifold.normal.y = 1.0f;
        } else {
            manifold.normal.y = -1.0f;
        }
    }

    return manifold;
}

i32 main(void) {
    GameState game_state = game_state_new();
    setup_ecs(&game_state);
    window_set_vsync(game_state.window, false);

    Font *font = font_init(str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), game_state.renderer, false, ALLOCATOR_LIBC);

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
            .gravity_multiplier = 0.0f,
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

    // Entity other = ecs_entity(game_state.ecs);
    // entity_add_component(game_state.ecs, other, Transform, {
    //         .pos = vec2(0.0f, 0.0f),
    //         .size = vec2(1.0f, 1.0f),
    //     });
    // entity_add_component(game_state.ecs, other, PhysicsBody, {
    //         .is_static = true,
    //     });
    // entity_add_component(game_state.ecs, other, Renderable, {
    //         .color = color_rgb_hex(0xff00ff),
    //         .texture = TEXTURE_NULL,
    //     });

    u32 fps = 0;
    f32 fps_timer = 0.0f;

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
            // ecs_run_group(game_state.ecs, game_state.fixed_group);

            // TODO: Implement spatial hashing and do collision detection/resolution
            // with swept aabb.

            // PhysicsWorld world = physics_world_init(&game_state);
            // physics_world_step(&world, &game_state, game_state.dt_fixed);
            // physics_world_update_ecs(&world, &game_state);
            // physics_world_free(&world);
        }
        // ecs_run_group(game_state.ecs, game_state.free_group);

        // Rendering
        glClearColor(color_arg(COLOR_BLACK));
        glClear(GL_COLOR_BUFFER_BIT);
        Camera game_cam = {
            .screen_size = window_get_size(game_state.window),
            .direction = vec2(1.0f, 1.0f),
            .position = vec2(0.0f, 0.0f),
            .zoom = 25.0f,
        };
        renderer_begin(game_state.renderer, game_cam);



        Quad a = {
            .pos = vec2(0.0f, 0.0f),
            .size = vec2(10.0f, 10.0f),
        };
        Quad b = {
            .pos = screen_to_world_space(game_cam, mouse_position(game_state.window)),
            .size = vec2(1.0f, 1.0f),
        };
        CollisionManifold manifold = colliding(a, b);



        // renderer_draw_quad(game_state.renderer, minkowski_diff, vec2s(0.0f), TEXTURE_NULL, colliding ? COLOR_RED : color_rgb_hex(0x808080));
        renderer_draw_quad(game_state.renderer, a, vec2s(0.0f), TEXTURE_NULL, manifold.is_colliding ? color_rgb_hex(0xa53030) : COLOR_WHITE);
        renderer_draw_quad(game_state.renderer, b, vec2s(0.0f), TEXTURE_NULL, COLOR_WHITE);
        if (manifold.is_colliding) {
            Quad resolved = {
                .pos = vec2_add(b.pos, manifold.depth),
                .size = b.size,
            };
            renderer_draw_quad(game_state.renderer, resolved, vec2s(0.0f), TEXTURE_NULL, color_rgb_hex(0x75a743));

            Quad normal = {
                .pos = a.pos,
                .size = vec2s(1.0f),
            };
            normal.pos = vec2_add(normal.pos, vec2_mul(vec2_divs(a.size, 2.0f), manifold.normal));
            normal.size = vec2_mul(normal.size, vec2_muls(manifold.normal, 1.0f));
            normal.size = vec2_adds(normal.size, 0.1f);
            renderer_draw_quad(game_state.renderer, normal, vec2s(0.0f), TEXTURE_NULL, color_rgb_hex(0x4f8fba));
        }

        renderer_end(game_state.renderer);
        renderer_submit(game_state.renderer);

        Ivec2 screen_size = window_get_size(game_state.window);
        Camera screen_cam = {
            .screen_size = screen_size,
            .zoom = screen_size.y,
            .position = vec2(vec2_arg(ivec2_divs(screen_size, 2))),
            .direction = vec2(1.0f, -1.0f),
        };
        renderer_begin(game_state.renderer, screen_cam);

        renderer_draw_string(game_state.renderer, str_lit("Intense gaming"), font, 32.0f, ivec2s(10), COLOR_WHITE);

        renderer_end(game_state.renderer);
        renderer_submit(game_state.renderer);

        window_poll_event(game_state.window);
        window_swap_buffers(game_state.window);
    }

    game_state_free(&game_state);
    return 0;
}
