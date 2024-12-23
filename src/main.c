#include "ecs.h"
#include "gfx.h"
#include "core.h"
#include "linear_algebra.h"
#include "window.h"
#include "ds.h"
#include <math.h>
#include <stdint.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <stdlib.h>

typedef enum {
    TILE_NONE,
    TILE_GROUND,

    TILE_TYPE_COUNT,
} TileType;

typedef struct Tile Tile;
struct Tile {
    TileType type;
    Color color;
};

#define WORLD_WIDTH 128
#define WORLD_HEIGHT 64

typedef struct DebugDraw DebugDraw;
struct DebugDraw {
    Quad quad;
    Color color;
};

typedef struct GameState GameState;
struct GameState {
    ECS *ecs;
    Window *window;
    Renderer *renderer;
    f32 dt;
    f32 gravity;
    Camera cam;

    SystemGroup group;

    Tile tiles[WORLD_WIDTH*WORLD_HEIGHT];

    DebugDraw debug_draw[1024];
    u32 debug_draw_i;
};

Tile get_tile(GameState *state, Vec2 pos) {
    Ivec2 idx = ivec2(roundf(pos.x), roundf(pos.y));
    if (idx.x < 0 || idx.y < 0 || idx.x >= WORLD_WIDTH || idx.y >= WORLD_HEIGHT) {
        // log_warn("Indexing out of world bounds.");
        return (Tile) {0};
    }
    return state->tiles[idx.x+idx.y*WORLD_WIDTH];
}

Tile *get_tiles_in_rect(GameState *state, Vec2 center, Vec2 half_size, Allocator allocator, Ivec2 *area) {
    Vec2 bl = vec2_sub(center, half_size);
    Vec2 tr = vec2_add(center, half_size);

    Ivec2 bl_idx = ivec2(roundf(bl.x), roundf(bl.y));
    Ivec2 tr_idx = ivec2(roundf(tr.x), roundf(tr.y));

    *area = ivec2_sub(tr_idx, bl_idx);
    Tile *tiles = allocator.alloc(area->x*area->y*sizeof(Tile), allocator.ctx);

    for (i32 y = 0; y < area->y; y++) {
        for (i32 x = 0; x < area->x; x++) {
            tiles[x+y*area->x] = get_tile(state, vec2(bl_idx.x+x, bl_idx.y+y));
        }
    }

    return tiles;
}

// -- Components ---------------------------------------------------------------

typedef struct Transform Transform;
struct Transform {
    Vec2 pos;
    Vec2 size;
};

typedef struct PlayerController PlayerController;
struct PlayerController {
    struct {
        f32 horizontal;
        b8 jumping;
        b8 jump_cancel;
    } input;
    f32 max_horizontal_speed;
    f32 acceleration;
    f32 deceleration;
    b8 grounded;

    f32 max_fall_speed;
    f32 max_flight_time;
    f32 flight_time;
    f32 max_vertical_speed;
    f32 flight_acc;

    f32 shoot_delay;
    f32 shoot_timer;
};

typedef struct Renderable Renderable;
struct Renderable {
    Color color;
    Texture texture;
};

typedef struct CollisionManifold CollisionManifold;
struct CollisionManifold {
    b8 is_colliding;
    Vec2 depth;
    Vec2 normal;
};

typedef void (*EntityCollisionCallback)(ECS *ecs, Entity self, Entity other, CollisionManifold manifold);
typedef void (*TileCollisionCallback)(ECS *ecs, Entity self, Vec2 tile_position, CollisionManifold manifold);

typedef struct PhysicsBody PhysicsBody;
struct PhysicsBody {
    f32 gravity_multiplier;
    Vec2 acceleration;
    Vec2 velocity;
    b8 is_static;
    b8 collider;
    EntityCollisionCallback entity_collision_cbs[16];
    TileCollisionCallback tile_collision_cbs[16];
};

typedef struct Projectile Projectile;
struct Projectile {
    f32 lifespan;
    i32 penetration;
    b8 friendly;
    b8 env_collide;
};

// -----------------------------------------------------------------------------

static void projectile_tile_collision(ECS *ecs, Entity self, Vec2 tile_position, CollisionManifold manifold) {
    (void) manifold;
    (void) tile_position;
    ecs_entity_kill(ecs, self);
}

// -- Systems ------------------------------------------------------------------

void template_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    for (u32 i = 0; i < iter.count; i++) {
    }
}

void player_input_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    PlayerController *controller = ecs_query_iter_get_field(iter, 0);
    for (u32 i = 0; i < iter.count; i++) {
        controller[i].input.horizontal = 0.0f;
        controller[i].input.horizontal -= key_down(state->window, KEY_A);
        controller[i].input.horizontal += key_down(state->window, KEY_D);

        controller[i].input.jumping = key_down(state->window, KEY_SPACE);

        if (key_release(state->window, KEY_SPACE)) {
            controller[i].input.jump_cancel = true;
        }
    }
}

void player_control_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    PlayerController *controller = ecs_query_iter_get_field(iter, 0);
    PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
    Transform *transform = ecs_query_iter_get_field(iter, 2);
    for (u32 i = 0; i < iter.count; i++) {
        // Grounded
        Vec2 under_player = transform[i].pos;
        under_player.y -= transform[i].size.y;
        f32 distance_to_ground = roundf(under_player.y) - transform[i].pos.y;
        controller[i].grounded = get_tile(state, under_player).type != TILE_NONE && distance_to_ground >= -1.0f;
        if (controller[i].grounded) {
            controller[i].flight_time = controller[i].max_flight_time;
        }

        // Horizontal movement
        body[i].velocity.x = controller[i].input.horizontal*controller[i].max_horizontal_speed;

        // Jumping
        if (controller[i].input.jumping && controller[i].grounded) {
            body[i].velocity.y = 30.0f;
            controller[i].grounded = false;
            controller[i].flight_time = controller[i].max_flight_time;
        }

        // Flying
        if (controller[i].input.jumping && controller[i].flight_time > 0.0f) {
            // Negate gravity
            body[i].velocity.y -= state->gravity*body[i].gravity_multiplier*state->dt;

            f32 current = body[i].velocity.y;
            f32 target = controller[i].max_vertical_speed;
            if (current < target) {
                f32 delta = target - current;
                body[i].velocity.y += controller[i].flight_acc*delta*state->dt;
            }
            controller[i].flight_time -= state->dt;
        }
        body[i].velocity.y = clamp(body[i].velocity.y, controller[i].max_fall_speed, controller[i].max_vertical_speed);

        // Auto step-up
        if (body[i].velocity.x != 0.0f) {
            Vec2 side_of_player = transform[i].pos;
            side_of_player.x += transform[i].size.x * sign(body[i].velocity.x);

            if (fabsf(roundf(side_of_player.x) - transform[i].pos.x) <= 1.0f) {
                Vec2 side_up_of_player = side_of_player;
                side_up_of_player.y += 1.0f;

                Tile side = get_tile(state, side_of_player);
                Tile side_up = get_tile(state, side_up_of_player);

                if (side.type != TILE_NONE && side_up.type == TILE_NONE) {
                    transform[i].pos.y += 1.0f;
                }
            }
        }

        // Shooting
        controller[i].shoot_timer += state->dt;
        if (mouse_button_down(state->window, MOUSE_BUTTON_LEFT) && controller[i].shoot_timer >= controller[i].shoot_delay) {
            controller[i].shoot_timer = 0.0f;

            Entity proj = ecs_entity(ecs);
            entity_add_component(ecs, proj, Transform, {
                    .pos = transform[i].pos,
                    .size = vec2s(0.5f),
                });
            entity_add_component(ecs, proj, Renderable, {
                    .color = COLOR_WHITE,
                });
            entity_add_component(ecs, proj, Projectile, {
                    .friendly = true,
                    .env_collide = true,
                    .penetration = 1,
                    .lifespan = -1.0f,
                });
            entity_add_component(ecs, proj, PhysicsBody, {
                    .gravity_multiplier = 10.0f,
                    .tile_collision_cbs = {
                        projectile_tile_collision
                    },
                });
        }
    }
}

void projectile_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Projectile *projectile = ecs_query_iter_get_field(iter, 0);
    for (u32 i = 0; i < iter.count; i++) {
        if (projectile[i].lifespan == -1.0f) {
            continue;
        }

        projectile[i].lifespan -= state->dt;
        if (projectile[i].lifespan <= 0.0f && projectile[i].penetration == 0) {
            Entity entity = ecs_query_iter_get_entity(iter, i);
            ecs_entity_kill(ecs, entity);
        }
    }
}

void camera_follow_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    for (u32 i = 0; i < iter.count; i++) {
        // state->cam.position = vec2_lerp(state->cam.position, transform->pos, state->dt*10.0f);
        state->cam.position = transform->pos;
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
        .gravity = -9.82f,
        .cam = {
            .direction = vec2(1.0f, 1.0f),
            .screen_size = window_get_size(window),
            .zoom = 50.0f,
        },
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
    state->group = ecs_system_group(state->ecs);

    ecs_register_component(state->ecs, Transform);
    ecs_register_component(state->ecs, PlayerController);
    ecs_register_component(state->ecs, Renderable);
    ecs_register_component(state->ecs, PhysicsBody);
    ecs_register_component(state->ecs, Projectile);

    ecs_register_system(state->ecs, player_input_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, PlayerController),
                QUERY_FIELDS_END,
            },
        });
    ecs_register_system(state->ecs, player_control_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, PlayerController),
                [1] = ecs_id(state->ecs, PhysicsBody),
                [2] = ecs_id(state->ecs, Transform),
                QUERY_FIELDS_END,
            },
        });

    ecs_register_system(state->ecs, projectile_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Projectile),
                QUERY_FIELDS_END,
            },
        });

    ecs_register_system(state->ecs, camera_follow_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Transform),
                [1] = ecs_id(state->ecs, PlayerController),
                QUERY_FIELDS_END,
            },
        });
}

void setup_world(GameState *state) {
    for (u32 x = 0; x < WORLD_WIDTH; x++) {
        u32 y = (sinf(rad(x))+1.0f)*5.0f;
        state->tiles[x+y*WORLD_WIDTH] = (Tile) {
            .type = TILE_GROUND,
            // .color = color_hsv(x*10.0f, 0.75f, 0.5f),
            .color = color_rgb_hex(0x212121)
        };
    }
}

typedef struct PhysicsObject PhysicsObject;
struct PhysicsObject {
    Entity id;
    Transform transform;
    PhysicsBody body;
};

typedef struct Cell Cell;
struct Cell {
    Vec(PhysicsObject) objs;
};

typedef struct PhysicsWorld PhysicsWorld;
struct PhysicsWorld {
    Query query;

    u32 cell_count;
    Vec2 cell_size;
    Cell *cells;
};

static CollisionManifold colliding(Transform a, Transform b) {
    CollisionManifold manifold = {0};

    // https://blog.hamaluik.ca/posts/simple-aabb-collision-using-minkowski-difference/
    Vec2 a_bottom_left = vec2_sub(a.pos, vec2_divs(a.size, 2.0f));
    Vec2 b_top_right = vec2_add(b.pos, vec2_divs(b.size, 2.0f));
    Vec2 size = vec2_add(a.size, b.size);
    Vec2 min = vec2_sub(a_bottom_left, b_top_right);
    Vec2 max = vec2_add(min, size);
    manifold.is_colliding = (
            min.x < 0.0f &&
            max.x > 0.0f &&
            min.y < 0.0f &&
            max.y > 0.0f
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

// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
static u64 hash_coord(i32 x, i32 y) {
    u64 hash = x & ((u64) y << 32);
    hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9UL;
    hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebUL;
    hash = hash ^ (hash >> 31);
    return hash;
}

PhysicsWorld physics_world_init(GameState *state) {
    u32 cell_count = 4096;
    PhysicsWorld world = {
        .cells = calloc(cell_count, sizeof(Cell)),
        .cell_count = cell_count,
        .cell_size = vec2s(5.0f),
    };

    // Pull entities out of the ecs.
    world.query = ecs_query(state->ecs, (QueryDesc) {
            .fields = {
                ecs_id(state->ecs, Transform),
                ecs_id(state->ecs, PhysicsBody),
                QUERY_FIELDS_END,
            },
        });
    for (u32 i = 0; i < world.query.count; i++) {
        QueryIter iter = ecs_query_get_iter(world.query, i);
        Transform *transform = ecs_query_iter_get_field(iter, 0);
        PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
        for (u32 j = 0; j < iter.count; j++) {
            PhysicsObject obj = {
                .id = ecs_query_iter_get_entity(iter, j),
                .transform = transform[j],
                .body = body[j],
            };

            Vec2 half_size = vec2_divs(obj.transform.size, 2.0f);
            Vec2 obj_sw = vec2_sub(obj.transform.pos, half_size);
            Vec2 obj_ne = vec2_add(obj.transform.pos, half_size);

            obj_sw = vec2_div(obj_sw, world.cell_size);
            obj_ne = vec2_div(obj_ne, world.cell_size);

            Ivec2 cell_sw = ivec2(roundf(obj_sw.x), roundf(obj_sw.y));
            Ivec2 cell_ne = ivec2(roundf(obj_ne.x), roundf(obj_ne.y));

            for (i32 y = cell_sw.y; y <= cell_ne.y; y++) {
                for (i32 x = cell_sw.x; x <= cell_ne.x; x++) {
                    u64 hash = hash_coord(x, y);
                    u32 index = hash % world.cell_count;
                    vec_push(world.cells[index].objs, obj);
                }
            }
        }
    }

    return world;
}

void physics_world_step(PhysicsWorld *world, GameState *state, f32 dt) {
    for (u32 i = 0; i < world->cell_count; i++) {
        for (u32 j = 0; j < vec_len(world->cells[i].objs); j++) {
            PhysicsObject *obj = &world->cells[i].objs[j];
            if (obj->body.is_static) {
                continue;
            }
            obj->body.acceleration.y += state->gravity*obj->body.gravity_multiplier;
            obj->body.velocity = vec2_add(obj->body.velocity, vec2_muls(obj->body.acceleration, dt));
            obj->transform.pos = vec2_add(obj->transform.pos, vec2_muls(obj->body.velocity, dt));
            obj->body.acceleration = vec2s(0.0f);
        }
    }

    for (u32 i = 0; i < world->cell_count; i++) {
        for (u32 j = 0; j < vec_len(world->cells[i].objs); j++) {
            PhysicsObject *obj = &world->cells[i].objs[j];
            if (obj->body.is_static) {
                continue;
            }

            // Tile collision
            Ivec2 area;
            Tile *tiles = get_tiles_in_rect(state, obj->transform.pos, vec2_adds(obj->transform.size, 1.0f), ALLOCATOR_LIBC, &area);
            for (i32 y = 0; y < area.y; y++) {
                for (i32 x = 0; x < area.x; x++) {
                    if (tiles[x+y*area.x].type == TILE_NONE) {
                        continue;
                    }

                    Vec2 pos = obj->transform.pos;
                    pos.x = roundf(pos.x - area.x / 2.0f) + x;
                    pos.y = roundf(pos.y - area.y / 2.0f) + y;
                    Transform tile_transform = {
                        .pos = pos,
                        .size = vec2s(1.0f),
                    };

                    CollisionManifold manifold = colliding(obj->transform, tile_transform);
                    state->debug_draw[state->debug_draw_i++] = (DebugDraw) {
                        .quad = {
                            .pos = tile_transform.pos,
                            .size = tile_transform.size,
                        },
                        .color = COLOR_BLUE,
                    };
                    if (manifold.is_colliding) {
                        obj->transform.pos = vec2_sub(obj->transform.pos, manifold.depth);
                        if (manifold.normal.y == -1.0f) {
                            obj->body.velocity.y = 0.0f;
                        }
                        // if (manifold.normal.x != 0.0f) {
                        //     if (manifold.normal.x > 0.0f && obj->body.velocity.x > 0.0f) {
                        //         obj->body.velocity.x = 0.0f;
                        //     } else if (manifold.normal.x < 0.0f && obj->body.velocity.x < 0.0f) {
                        //         obj->body.velocity.x = 0.0f;
                        //     }
                        // } else {
                        //     obj->transform.pos.y -= manifold.depth.y;
                        //     if (manifold.normal.y > 0.0f && obj->body.velocity.y > 0.0f) {
                        //         obj->body.velocity.y = 0.0f;
                        //     } else if (manifold.normal.y < 0.0f && obj->body.velocity.y < 0.0f) {
                        //         obj->body.velocity.y = 0.0f;
                        //     }
                        // }
                    }
                }
            }
            state->debug_draw[state->debug_draw_i++] = (DebugDraw) {
                .quad = {
                    .pos = obj->transform.pos,
                    .size = obj->transform.size,
                },
                .color = COLOR_GREEN,
            };
            free(tiles);
        }
    }

    // Collision detection.
    // for (u32 i = 0; i < world->cell_count; i++) {
    //     for (u32 j = 0; j < vec_len(world->cells[i].objs); j++) {
    //         for (u32 k = 0; k < vec_len(world->cells[i].objs); k++) {
    //             if (j == k) {
    //                 continue;
    //             }
    //
    //             PhysicsObject *a = &world->cells[i].objs[j];
    //             PhysicsObject *b = &world->cells[i].objs[k];
    //
    //             if (a->body.is_static) {
    //                 continue;
    //             }
    //
    //             CollisionManifold manifold = colliding(a->transform, b->transform);
    //             if (!manifold.is_colliding) {
    //                 continue;
    //             }
    //
    //             if (b->body.is_static && b->body.collider && a->body.collider) {
    //                 if (manifold.normal.x != 0.0f) {
    //                     a->transform.pos.x -= manifold.depth.x;
    //                     if (manifold.normal.x > 0.0f && a->body.velocity.x > 0.0f) {
    //                         a->body.velocity.x = 0.0f;
    //                     } else if (manifold.normal.x < 0.0f && a->body.velocity.x < 0.0f) {
    //                         a->body.velocity.x = 0.0f;
    //                     }
    //                 } else {
    //                     a->transform.pos.y -= manifold.depth.y;
    //                     if (manifold.normal.y > 0.0f && a->body.velocity.y > 0.0f) {
    //                         a->body.velocity.y = 0.0f;
    //                     } else if (manifold.normal.y < 0.0f && a->body.velocity.y < 0.0f) {
    //                         a->body.velocity.y = 0.0f;
    //                     }
    //                 }
    //             }
    //
    //             for (u32 l = 0; l < arrlen(a->body.collision_cbs); l++) {
    //                 if (a->body.collision_cbs[l] == NULL) {
    //                     break;
    //                 }
    //                 a->body.collision_cbs[l](state->ecs, a->id, b->id, manifold);
    //             }
    //
    //             for (u32 l = 0; l < arrlen(b->body.collision_cbs); l++) {
    //                 if (b->body.collision_cbs[l] == NULL) {
    //                     break;
    //                 }
    //                 b->body.collision_cbs[l](state->ecs, b->id, a->id, manifold);
    //             }
    //         }
    //     }
    // }
}

void physics_world_update_ecs(PhysicsWorld *world, GameState *state) {
    for (u32 i = 0; i < world->cell_count; i++) {
        for (u32 j = 0; j < vec_len(world->cells[i].objs); j++) {
            PhysicsObject obj = world->cells[i].objs[j];
            Transform *transform = entity_get_component(state->ecs, obj.id, Transform);
            PhysicsBody *body = entity_get_component(state->ecs, obj.id, PhysicsBody);
            *transform = obj.transform;
            *body = obj.body;
        }
    }
}

void physics_world_free(GameState *state, PhysicsWorld *world) {
    ecs_query_free(state->ecs, world->query);
    for (u32 i = 0; i < world->cell_count; i++) {
        vec_free(world->cells[i].objs);
    }
    free(world->cells);
}

i32 main(void) {
    GameState game_state = game_state_new();
    setup_ecs(&game_state);
    setup_world(&game_state);
    window_set_vsync(game_state.window, false);

    Font *font = font_init(str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), game_state.renderer, false, ALLOCATOR_LIBC);

    Entity player = ecs_entity(game_state.ecs);
    entity_add_component(game_state.ecs, player, Transform, {
            .pos = vec2(WORLD_WIDTH/2.0f, WORLD_HEIGHT/8.0f),
            .size = vec2(1.0f, 1.0f),
        });
    entity_add_component(game_state.ecs, player, PlayerController, {
            .acceleration = 5.0f,
            .deceleration = 40.0f,
            .max_horizontal_speed = 30.0f,

            .max_fall_speed = -90.0f,
            .max_flight_time = 10.0f,
            .max_vertical_speed = 30.0f,
            .flight_acc = 5.0f,

            .shoot_delay = 0.1f,
        });
    entity_add_component(game_state.ecs, player, Renderable, {
            .color = color_hsv(0.0f, 0.75f, 1.0f),
            .texture = TEXTURE_NULL,
        });
    entity_add_component(game_state.ecs, player, PhysicsBody, {
            .gravity_multiplier = 10.0f,
            .is_static = false,
            .collider = true,
        });

    u32 fps = 0;
    f32 fps_timer = 0.0f;

    f32 last = glfwGetTime();
    while (window_is_open(game_state.window)) {
        f32 curr = glfwGetTime();
        game_state.dt = curr - last;
        last = curr;

        fps++;
        fps_timer += game_state.dt;
        if (fps_timer >= 1.0f) {
            log_info("FPS: %u", fps);
            fps = 0;
            fps_timer = 0.0f;
        }

        game_state.cam.screen_size = window_get_size(game_state.window);

        // Game logic
        game_state.debug_draw_i = 0;

        ecs_run_group(game_state.ecs, game_state.group);

        PhysicsWorld world = physics_world_init(&game_state);
        physics_world_step(&world, &game_state, game_state.dt);
        physics_world_update_ecs(&world, &game_state);
        physics_world_free(&game_state, &world);

        // Rendering
        glClearColor(color_arg(COLOR_BLACK));
        glClear(GL_COLOR_BUFFER_BIT);
        renderer_begin(game_state.renderer, game_state.cam);

        for (u32 y = 0; y < WORLD_HEIGHT; y++) {
            for (u32 x = 0; x < WORLD_WIDTH; x++) {
                Tile tile = game_state.tiles[x+y*WORLD_WIDTH];
                switch (tile.type) {
                    case TILE_NONE:
                        break;

                    case TILE_GROUND:
                        renderer_draw_quad(game_state.renderer, (Quad) {
                                .pos = vec2(x, y),
                                .size = vec2s(1.0f),
                            }, vec2s(0.0f), TEXTURE_NULL, tile.color);

                    case TILE_TYPE_COUNT:
                        break;
                }
            }
        }

        Query query = ecs_query(game_state.ecs, (QueryDesc) {
                .fields = {
                    ecs_id(game_state.ecs, Transform),
                    ecs_id(game_state.ecs, Renderable),
                    QUERY_FIELDS_END,
                },
            });

        for (u32 i = 0; i < query.count; i++) {
            QueryIter iter = ecs_query_get_iter(query, i);
            Transform *t = ecs_query_iter_get_field(iter, 0);
            Renderable *r = ecs_query_iter_get_field(iter, 1);
            for (u32 j = 0; j < iter.count; j++) {
                renderer_draw_quad(game_state.renderer, (Quad) {
                        .pos = t[j].pos,
                        .size = t[j].size,
                    }, vec2s(0.0f), r[j].texture, r[j].color);
            }
        }
        ecs_query_free(game_state.ecs, query);

        for (u32 i = 0; i < game_state.debug_draw_i; i++) {
            DebugDraw draw = game_state.debug_draw[i];
            renderer_draw_quad(game_state.renderer,
                    draw.quad,
                    vec2s(0.0f),
                    TEXTURE_NULL,
                    draw.color);
        }

        renderer_end(game_state.renderer);
        renderer_submit(game_state.renderer);

        // UI
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
