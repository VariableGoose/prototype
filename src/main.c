#include "ecs.h"
#include "gfx.h"
#include "core.h"
#include "window.h"
#include "ds.h"
#include <stdio.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <stdlib.h>

// -- Components ---------------------------------------------------------------

typedef AABB Transform;

typedef struct Player Player;
struct Player {
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

typedef void (*EntityCollisionCallback)(ECS *ecs, Entity self, Entity other, MinkowskiDifference manifold);
typedef void (*TileCollisionCallback)(ECS *ecs, Entity self, Vec2 tile_position, MinkowskiDifference manifold);

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
    i32 damage;
};

typedef enum {
    ENEMY_AI_NONE,
    ENEMY_AI_SLIME,
    ENEMY_AI_BOSS,
} EnemyAI;

typedef struct Enemy Enemy;
struct Enemy {
    EnemyAI ai;
    Entity target;
    f32 shoot_timer;
    f32 shoot_delay;
    f32 jump_timer;
    f32 jump_delay;
    b8 invincible;
};

typedef struct Health Health;
struct Health {
    i32 max;
    i32 curr;
};

typedef struct Hit Hit;
struct Hit {
    Color color;
    i32 damage;
    f32 timer;
};

typedef enum {
    BOSS_ATTACK_CARPET_BOMB,
    BOSS_ATTACK_SHIELD,
    BOSS_ATTACK_TASTE_THE_RAINBOW,
} BossAttack;

typedef struct Boss Boss;
struct Boss {
    BossAttack attack;
    f32 attack_timer;
    b8 shielded;
    Vec(Entity) shields;
    // ttr = taste the rainbow
    u32 ttr_circle_count;
};

// -----------------------------------------------------------------------------

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
    AABB aabb;
    Color color;
};

typedef Vec(Entity) Cell;

typedef struct SpatialGrid SpatialGrid;
struct SpatialGrid {
    Allocator allocator;
    u32 cell_count;
    Vec2 cell_size;
    Cell *cells;
};

// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
static u64 hash_coord(i32 x, i32 y) {
    u64 hash = x | ((u64) y << 32);
    hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9UL;
    hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebUL;
    hash = hash ^ (hash >> 31);
    return hash;
}

Cell *grid_get_cell(SpatialGrid *grid, i32 x, i32 y) {
    u64 hash = hash_coord(x, y);
    u32 index = hash % grid->cell_count;
    return &grid->cells[index];
}

SpatialGrid grid_new(u32 cell_count, Vec2 cell_size, Allocator allocator) {
    Cell *cells = allocator.alloc(sizeof(Cell)*cell_count, allocator.ctx);
    memset(cells, 0, sizeof(Cell)*cell_count);
    return (SpatialGrid) {
        .allocator = allocator,
        .cell_count = cell_count,
        .cell_size = cell_size,
        .cells = cells,
    };
}

void grid_free(SpatialGrid *grid) {
    for (u32 i = 0; i < grid->cell_count; i++) {
        vec_free(grid->cells[i]);
    }
    grid->allocator.free(grid->cells, sizeof(Cell)*grid->cell_count, grid->allocator.ctx);
}

void grid_insert(SpatialGrid *grid, ECS *ecs, Entity entity) {
    Transform *transform = entity_get_component(ecs, entity, Transform);
    if (transform == NULL) {
        log_warn("Partitioning an entity without a transform.");
        return;
    }

    Vec2 half_size = aabb_half_size(*transform);
    Vec2 obj_sw = vec2_sub(transform->position, half_size);
    Vec2 obj_ne = vec2_add(transform->position, half_size);

    obj_sw = vec2_div(obj_sw, grid->cell_size);
    obj_ne = vec2_div(obj_ne, grid->cell_size);

    Ivec2 cell_sw = ivec2(roundf(obj_sw.x), roundf(obj_sw.y));
    Ivec2 cell_ne = ivec2(roundf(obj_ne.x), roundf(obj_ne.y));

    for (i32 y = cell_sw.y; y <= cell_ne.y; y++) {
        for (i32 x = cell_sw.x; x <= cell_ne.x; x++) {
            Cell *cell = grid_get_cell(grid, x, y);
            vec_push(*cell, entity);
        }
    }
}

void grid_clear(SpatialGrid *grid) {
    for (u32 i = 0; i < grid->cell_count; i++) {
        vec_clear(grid->cells[i]);
    }
}

Vec(Entity) grid_query_radius(SpatialGrid *grid, ECS *ecs, Vec2 pos, f32 radius) {
    Vec2 grid_min = vec2_sub(pos, vec2s(radius));
    Vec2 grid_max = vec2_add(pos, vec2s(radius));
    grid_min = vec2_div(grid_min, grid->cell_size);
    grid_max = vec2_div(grid_max, grid->cell_size);

    Vec(Entity) result = NULL;
    for (i32 y = floorf(grid_min.y); y < ceilf(grid_max.y); y++) {
        for (i32 x = floorf(grid_min.x); x < ceilf(grid_max.x); x++) {
            Cell *cell = grid_get_cell(grid, x, y);
            for (u32 i = 0; i < vec_len(*cell); i++) {
                Entity ent = (*cell)[i];
                if (!entity_alive(ecs, ent)) {
                    continue;
                }

                Transform *transform = entity_get_component(ecs, ent, Transform);
                if (aabb_overlap_circle(*transform, pos, radius)) {
                    vec_push(result, ent);
                }
            }
        }
    }
    return result;
}

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

    SpatialGrid grid;

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

static void projectile_tile_collision(ECS *ecs, Entity self, Vec2 tile_position, MinkowskiDifference manifold) {
    (void) manifold;
    (void) tile_position;
    Projectile *proj = entity_get_component(ecs, self, Projectile);
    if (proj->env_collide) {
        ecs_entity_kill(ecs, self);
    }
}

static void projectile_entity_collision(ECS *ecs, Entity self, Entity other, MinkowskiDifference manifold) {
    (void) manifold;
    Projectile *proj = entity_get_component(ecs, self, Projectile);
    Transform *proj_transform = entity_get_component(ecs, self, Transform);

    if (proj->friendly) {
        Enemy *enemy = entity_get_component(ecs, other, Enemy);
        Health *health = entity_get_component(ecs, other, Health);
        if (enemy == NULL || enemy->invincible) {
            return;
        }
        proj->penetration -= 1;

        if (health != NULL) {
            health->curr -= proj->damage;
            Entity hit = ecs_entity(ecs);
            entity_add_component(ecs, hit, Transform, {
                    .position = proj_transform->position,
                });
            entity_add_component(ecs, hit, Hit, {
                    .damage = proj->damage,
                    .color = COLOR_RED,
                });
        }

        if (proj->penetration == 0) {
            ecs_entity_kill(ecs, self);
        }
    } else {
        Player *player = entity_get_component(ecs, other, Player);
        Health *health = entity_get_component(ecs, other, Health);
        if (player == NULL) {
            return;
        }
        proj->penetration -= 1;

        if (health != NULL) {
            health->curr -= proj->damage;
            Entity hit = ecs_entity(ecs);
            entity_add_component(ecs, hit, Transform, {
                    .position = proj_transform->position,
                });
            entity_add_component(ecs, hit, Hit, {
                    .damage = proj->damage,
                    .color = COLOR_RED,
                });
        }

        if (proj->penetration == 0) {
            ecs_entity_kill(ecs, self);
        }
    }
}

b8 is_grounded(GameState *state, Transform transform) {
    Vec2 half_size = aabb_half_size(transform);
    Vec2 under = transform.position;
    under.y -= half_size.y;
    under.y = floorf(under.y);

    i32 x_min = roundf(transform.position.x - half_size.x);
    i32 x_max = roundf(transform.position.x + half_size.x);

    b8 has_tile_under = false;
    for (i32 x = x_min; x <= x_max; x++) {
        under.x = x;

        if (get_tile(state, under).type != TILE_NONE) {
            has_tile_under = true;
            break;
        }
    }

    if (!has_tile_under) {
        return false;
    }

    f32 diff = transform.position.y - under.y;
    // For the transform to touch the tile the distance from the center of the
    // transform to the center of the tile must be less or equal to half of the
    // transform size and half of the tile size (0.5).
    return diff <= half_size.y + 0.5f;
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

    Player *controller = ecs_query_iter_get_field(iter, 0);
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

    Player *controller = ecs_query_iter_get_field(iter, 0);
    PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
    Transform *transform = ecs_query_iter_get_field(iter, 2);
    for (u32 i = 0; i < iter.count; i++) {
        // Grounded
        controller[i].grounded = is_grounded(state, transform[i]);
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
            Vec2 side_of_player = transform[i].position;
            side_of_player.x += transform[i].size.x * sign(body[i].velocity.x);

            if (fabsf(roundf(side_of_player.x) - transform[i].position.x) <= 1.0f) {
                Vec2 side_up_of_player = side_of_player;
                side_up_of_player.y += 1.0f;

                Tile side = get_tile(state, side_of_player);
                Tile side_up = get_tile(state, side_up_of_player);

                if (side.type != TILE_NONE && side_up.type == TILE_NONE) {
                    transform[i].position.y += 1.0f;
                }
            }
        }

        // Shooting
        controller[i].shoot_timer += state->dt;
        if (mouse_button_down(state->window, MOUSE_BUTTON_LEFT) && controller[i].shoot_timer >= controller[i].shoot_delay) {
            controller[i].shoot_timer = 0.0f;

            Vec2 mouse = screen_to_world_space(state->cam, mouse_position(state->window));
            Vec2 player = transform[i].position;
            Vec2 dir = vec2_sub(mouse, player);
            dir = vec2_normalized(dir);
            dir = vec2_muls(dir, 30.0f);

            Entity proj = ecs_entity(ecs);
            entity_add_component(ecs, proj, Transform, {
                    .position = transform[i].position,
                    .size = vec2s(0.5f),
                });
            entity_add_component(ecs, proj, Renderable, {
                    .color = COLOR_WHITE,
                });
            entity_add_component(ecs, proj, Projectile, {
                    .friendly = true,
                    .env_collide = true,
                    .penetration = 1,
                    .lifespan = 3.0f,
                    .damage = 5,
                });
            entity_add_component(ecs, proj, PhysicsBody, {
                    .gravity_multiplier = 0.0f,
                    .velocity = dir,
                    .collider = true,
                    .tile_collision_cbs = {
                        projectile_tile_collision
                    },
                    .entity_collision_cbs = {
                        projectile_entity_collision
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
        if (projectile[i].lifespan <= 0.0f || projectile[i].penetration == 0) {
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
        // state->cam.position = vec2_lerp(state->cam.position, transform->position, state->dt*10.0f);
        state->cam.position = transform->position;
    }
}

void physics_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        if (body[i].is_static) {
            continue;
        }

        body[i].acceleration.y += state->gravity*body[i].gravity_multiplier;
        body[i].velocity = vec2_add(body[i].velocity, vec2_muls(body[i].acceleration, state->dt));
        transform[i].position = vec2_add(transform[i].position, vec2_muls(body[i].velocity, state->dt));
        body[i].acceleration = vec2s(0.0f);
    }
}

void tile_collision_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    PhysicsBody *body = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        if (body[i].is_static || !body[i].collider) {
            continue;
        }

        // Tile collision
        Ivec2 area;
        Tile *tiles = get_tiles_in_rect(state, transform[i].position, vec2_adds(transform[i].size, 1.0f), ALLOCATOR_LIBC, &area);
        for (i32 y = 0; y < area.y; y++) {
            for (i32 x = 0; x < area.x; x++) {
                if (tiles[x+y*area.x].type == TILE_NONE) {
                    continue;
                }

                Vec2 pos = transform[i].position;
                pos.x = roundf(pos.x - area.x / 2.0f) + x;
                pos.y = roundf(pos.y - area.y / 2.0f) + y;
                Transform tile_transform = {
                    .position = pos,
                    .size = vec2s(1.0f),
                };

                MinkowskiDifference diff = aabb_minkowski_difference(transform[i], tile_transform);
                if (diff.is_overlapping) {
                    transform[i].position = vec2_sub(transform[i].position, diff.depth);
                    if (diff.normal.y <= -1.0f) {
                        body[i].velocity.y = 0.0f;
                    }

                    for (u32 j = 0; j < arrlen(body[i].tile_collision_cbs); j++) {
                        if (body[i].tile_collision_cbs[j] != NULL) {
                            Entity ent = ecs_query_iter_get_entity(iter, i);
                            body[i].tile_collision_cbs[j](ecs, ent, pos, diff);
                        }
                    }
                }
            }
        }

        free(tiles);
    }
}

void entity_to_entity_collision(GameState *state) {
    ECS *ecs = state->ecs;
    SpatialGrid grid = state->grid;
    for (u32 i = 0; i < grid.cell_count; i++) {
        u32 len = vec_len(grid.cells[i]);
        if (len == 0) {
            continue;
        }
        for (u32 j = 0; j < len - 1; j++) {
            Entity a = grid.cells[i][j];
            if (!entity_alive(ecs, a)) {
                continue;
            }
            Transform *a_transform = entity_get_component(ecs, a, Transform);
            PhysicsBody *a_body = entity_get_component(ecs, a, PhysicsBody);
            if (a_transform == NULL) {
                log_error("Entity without transform in spatial grid.");
                continue;
            }
            if (a_body == NULL) {
                continue;
            }

            for (u32 k = j + 1; k < len; k++) {
                Entity b = grid.cells[i][k];
                if (!entity_alive(ecs, b)) {
                    continue;
                }
                Transform *b_transform = entity_get_component(ecs, b, Transform);
                PhysicsBody *b_body = entity_get_component(ecs, b, PhysicsBody);
                if (b_transform == NULL) {
                    log_error("Entity without transform in spatial grid.");
                    continue;
                }
                if (b_body == NULL) {
                    continue;
                }

                MinkowskiDifference diff = aabb_minkowski_difference(*a_transform, *b_transform);
                if (!diff.is_overlapping) {
                    continue;
                }

                for (u32 l = 0; l < arrlen(a_body->entity_collision_cbs); l++) {
                    if (a_body->entity_collision_cbs[l] == NULL) {
                        break;
                    }
                    a_body->entity_collision_cbs[l](ecs, a, b, diff);
                }

                for (u32 l = 0; l < arrlen(b_body->entity_collision_cbs); l++) {
                    if (b_body->entity_collision_cbs[l] == NULL) {
                        break;
                    }
                    b_body->entity_collision_cbs[l](ecs, b, a, diff);
                }
            }
        }
    }
}

void slime_ai(GameState *state, Entity slime, Transform *transform, Enemy *enemy) {
    ECS *ecs = state->ecs;

    PhysicsBody *body = entity_get_component(ecs, slime, PhysicsBody);
    // Deceleration
    body->velocity.x = lerp(body->velocity.x, 0.0f, state->dt*2.0f);

    // Sarch for target
    if (enemy->target == (Entity) -1) {
        Vec(Entity) near = grid_query_radius(&state->grid, ecs, transform->position, 30.0f);
        for (u32 i = 0; i < vec_len(near); i++) {
            Player *player = entity_get_component(ecs, near[i], Player);
            if (player != NULL) {
                enemy->target = near[i];
                break;
            }
        }
        vec_free(near);
    } else {
        Transform *target_transform = entity_get_component(ecs, enemy->target, Transform);

        // Jumping
        enemy->jump_timer += state->dt;
        if (is_grounded(state, *transform) && enemy->jump_timer >= enemy->jump_delay) {
            enemy->jump_timer = 0.0f;
            f32 target_dir = target_transform->position.x - transform->position.x;
            body->velocity = vec2(20.0f*sign(target_dir), 50.0f);
        }

        // Shoot towards target
        enemy->shoot_timer += state->dt;
        if (enemy->shoot_timer >= enemy->shoot_delay) {
            enemy->shoot_timer = 0.0f;

            Vec2 dir = vec2_sub(target_transform->position, transform->position);
            dir = vec2_normalized(dir);
            dir = vec2_muls(dir, 20.0f);

            Entity proj = ecs_entity(ecs);
            entity_add_component(ecs, proj, Transform, {
                    .position = transform->position,
                    .size = vec2s(0.5f),
                });
            entity_add_component(ecs, proj, Renderable, {
                    .color = color_rgb_hex(0xfcba03),
                });
            entity_add_component(ecs, proj, Projectile, {
                    .friendly = false,
                    .env_collide = true,
                    .penetration = 1,
                    .lifespan = 3.0f,
                    .damage = 2,
                });
            entity_add_component(ecs, proj, PhysicsBody, {
                    .collider = true,
                    .gravity_multiplier = 0.0f,
                    .velocity = dir,
                    .tile_collision_cbs = {
                        projectile_tile_collision
                    },
                    .entity_collision_cbs = {
                        projectile_entity_collision
                    },
                });
        }
    }
}

void attack_carpet_bomb(GameState *state, Entity ent, Transform *transform, Enemy *enemy, Boss *boss) {
    ECS *ecs = state->ecs;
    PhysicsBody *body = entity_get_component(ecs, ent, PhysicsBody);
    Transform *target_transform = entity_get_component(ecs, enemy->target, Transform);
    PhysicsBody *target_body = entity_get_component(ecs, enemy->target, PhysicsBody);

    Vec2 half_size = aabb_half_size(*transform);

    // Move to target position
    Vec2 target_pos = target_transform->position;
    Vec2 target_half_size = aabb_half_size(*target_transform);
    target_pos.y += target_half_size.y;
    target_pos.y += half_size.y;
    target_pos.y += 16;

    f32 bomb_gravity_mult = 10.0f;
    f32 bomb_v0 = 0.0f;
    f32 dist_to_player = -fabsf(target_transform->position.x - transform->position.x);

    f32 a = bomb_gravity_mult*state->gravity;
    f32 t1 = -bomb_v0 / a + sqrtf(powf(bomb_v0 / a, 2.0f) + (2.0f * dist_to_player) / a);

    target_pos.x += target_body->velocity.x * t1;

    if (transform->position.x != target_pos.x ||
            transform->position.y != target_pos.y) {
        Vec2 dir = vec2_sub(target_pos, transform->position);
        dir = vec2_normalized(dir);
        dir = vec2_muls(dir, 100.0f);
        body->velocity = dir;
    } else {
        body->velocity = vec2s(0.0f);
    }

    // Drop bombs
    enemy->shoot_timer += state->dt;
    if (enemy->shoot_timer >= enemy->shoot_delay) {
        enemy->shoot_timer = 0.0f;

        Entity bomb = ecs_entity(ecs);
        entity_add_component(ecs, bomb, Transform, {
                .position = transform->position,
                .size = vec2s(0.5f),
            });
        entity_add_component(ecs, bomb, Renderable, {
                .color = color_rgb_hex(0x808080),
            });
        entity_add_component(ecs, bomb, Projectile, {
                .friendly = false,
                .env_collide = false,
                .penetration = 1,
                .lifespan = 3.0f,
                .damage = 20,
            });
        entity_add_component(ecs, bomb, PhysicsBody, {
                .gravity_multiplier = bomb_gravity_mult,
                .velocity = {
                    .y = bomb_v0,
                },
                .tile_collision_cbs = {
                    projectile_tile_collision
                },
                .entity_collision_cbs = {
                    projectile_entity_collision
                },
            });
    }

    boss->attack_timer += state->dt;
    if (boss->attack_timer >= 10.0f) {
        body->velocity = vec2s(0.0f);
        boss->attack_timer = 0.0f;
        enemy->shoot_timer = 0.0f;
        boss->attack = BOSS_ATTACK_SHIELD;
    }
}

Entity spawn_shield(ECS *ecs, Vec2 pos) {
    Entity ent = ecs_entity(ecs);
    entity_add_component(ecs, ent, Transform, {
            .position = pos,
            .size = vec2s(1.0f),
        });
    entity_add_component(ecs, ent, Renderable, {
            .color = color_rgb_hex(0x9ed0ff),
        });
    entity_add_component(ecs, ent, Enemy, {0});
    entity_add_component(ecs, ent, Health, {
            .max = 25.0f,
            .curr = 25.0f,
        });
    entity_add_component(ecs, ent, PhysicsBody, {
            .collider = true,
            .gravity_multiplier = 0.0f,
        });

    return ent;
}

void spawn_slime(ECS *ecs, Vec2 pos, f32 jump_delay) {
    Entity ent = ecs_entity(ecs);
    Vec2 dir = vec2(cosf(jump_delay), sinf(jump_delay));
    dir = vec2_muls(dir, 25.0f);
    entity_add_component(ecs, ent, Transform, {
            .position = pos,
            .size = vec2s(1.0f),
        });
    entity_add_component(ecs, ent, Renderable, {
            .color = color_rgb_hex(0xfcba03),
        });
    entity_add_component(ecs, ent, Enemy, {
            .ai = ENEMY_AI_SLIME,
            .jump_delay = jump_delay,
            .shoot_delay = 1.0f,
        });
    entity_add_component(ecs, ent, Health, {
            .max = 25.0f,
            .curr = 25.0f,
        });
    entity_add_component(ecs, ent, PhysicsBody, {
            .collider = true,
            .velocity = dir,
            .gravity_multiplier = 10.0f,
        });
}

void attack_shield(GameState *state, Entity ent, Transform *transform, Enemy *enemy, Boss *boss) {
    ECS *ecs = state->ecs;

    if (!boss->shielded) {
        boss->shielded = true;
        Renderable *renderable = entity_get_component(ecs, ent, Renderable);
        renderable->color = color_rgb_hex(0x19161f);
        enemy->invincible = true;

        const u32 shield_count = 6;
        for (u32 i = 0; i < shield_count; i++) {
            const f32 radius = 10.0f;
            Vec2 shield_pos = transform->position;
            shield_pos.x += cosf((2.0f * PI / shield_count) * i) * radius;
            shield_pos.y += sinf((2.0f * PI / shield_count) * i) * radius;
            Entity shield_ent = spawn_shield(ecs, shield_pos);
            vec_push(boss->shields, shield_ent);
        }

        const u32 slime_count = 4;
        for (u32 i = 0; i < slime_count; i++) {
            spawn_slime(ecs, transform->position, 1.0f + 0.25f * (i + 1));
        }
    }

    b8 has_live_shields = false;
    for (u32 i = 0; i < vec_len(boss->shields); i++) {
        if (entity_alive(ecs, boss->shields[i])) {
            has_live_shields = true;
            break;
        }
    }

    if (!has_live_shields) {
        boss->shielded = false;
        Renderable *renderable = entity_get_component(ecs, ent, Renderable);
        renderable->color = color_rgb_hex(0x4e03fc);
        enemy->invincible = false;
        boss->attack = BOSS_ATTACK_TASTE_THE_RAINBOW;
    }
}

void attack_taste_the_rainbow(GameState *state, Entity ent, Transform *transform, Enemy *enemy, Boss *boss) {
    ECS *ecs = state->ecs;
    PhysicsBody *body = entity_get_component(ecs, ent, PhysicsBody);

    const f32 epsilon = 0.01f;
    const Vec2 target_pos = vec2(WORLD_WIDTH/2.0f, WORLD_HEIGHT/2.0f);
    if (fabsf(target_pos.x - transform->position.x) > epsilon ||
        fabsf(target_pos.y - transform->position.y) > epsilon) {
        Vec2 dir = vec2_sub(target_pos, transform->position);
        dir = vec2_normalized(dir);
        dir = vec2_muls(dir, 100.0f);
        body->velocity = dir;
        return;
    }
    body->velocity = vec2s(0.0f);

    const u32 circle_count = 32;
    enemy->shoot_timer += state->dt;
    if (enemy->shoot_timer >= 0.1f) {
        enemy->shoot_timer = 0.0f;

        boss->ttr_circle_count++;

        const u32 proj_count = 16;
        for (u32 i = 0; i < proj_count; i++) {
            Entity proj = ecs_entity(ecs);
            Color color = color_hsv(360.0f / proj_count * i, 0.75f, 1.0f);

            f32 angle_a = 2.0f * PI / proj_count * i;
            f32 angle_b = (PI/12.0f) * boss->ttr_circle_count;
            Vec2 dir = vec2(cosf(angle_a + angle_b), sinf(angle_a + angle_b));
            dir = vec2_muls(dir, 25.0f);

            entity_add_component(ecs, proj, Transform, {
                    .position = transform->position,
                    .size = vec2s(0.5f),
                });
            entity_add_component(ecs, proj, Renderable, {
                    .color = color,
                });
            entity_add_component(ecs, proj, Projectile, {
                    .friendly = false,
                    .env_collide = true,
                    .penetration = 1,
                    .lifespan = 10.0f,
                    .damage = 5,
                });
            entity_add_component(ecs, proj, PhysicsBody, {
                    .gravity_multiplier = 0.0f,
                    .velocity = dir,
                    .collider = true,
                    .tile_collision_cbs = {
                        projectile_tile_collision
                    },
                    .entity_collision_cbs = {
                        projectile_entity_collision
                    },
                });
        }
    }

    if (boss->ttr_circle_count == circle_count) {
        boss->ttr_circle_count = 0;
        boss->attack = BOSS_ATTACK_CARPET_BOMB;
    }
}

void boss_ai(GameState *state, Entity ent, Transform *transform, Enemy *enemy) {
    ECS *ecs = state->ecs;
    Boss *boss = entity_get_component(ecs, ent, Boss);

    // Find the target and never change.
    if (enemy->target == (Entity) -1) {
        Vec(Entity) near = grid_query_radius(&state->grid, ecs, transform->position, 30.0f);
        for (u32 i = 0; i < vec_len(near); i++) {
            Player *player = entity_get_component(ecs, near[i], Player);
            if (player != NULL) {
                enemy->target = near[i];
                break;
            }
        }
        vec_free(near);

        if (enemy->target == (Entity) -1) {
            return;
        }
    }

    switch (boss->attack) {
        case BOSS_ATTACK_CARPET_BOMB:
            attack_carpet_bomb(state, ent, transform, enemy, boss);
            break;
        case BOSS_ATTACK_SHIELD:
            attack_shield(state, ent, transform, enemy, boss);
            break;
        case BOSS_ATTACK_TASTE_THE_RAINBOW:
            attack_taste_the_rainbow(state, ent, transform, enemy, boss);
            break;
    }
}

void enemy_ai(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) ecs;
    GameState *state = user_ptr;

    Transform *transform = ecs_query_iter_get_field(iter, 0);
    Enemy *enemy = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        Entity ent = ecs_query_iter_get_entity(iter, i);
        switch (enemy[i].ai) {
            case ENEMY_AI_NONE:
                break;
            case ENEMY_AI_SLIME:
                slime_ai(state, ent, &transform[i], &enemy[i]);
                break;
            case ENEMY_AI_BOSS:
                boss_ai(state, ent, &transform[i], &enemy[i]);
                break;
        }
    }
}

void health_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    (void) user_ptr;

    Health *h = ecs_query_iter_get_field(iter, 0);
    for (u32 i = 0; i < iter.count; i++) {
        h[i].curr = clamp(h[i].curr, 0, h[i].max);
        if (h[i].curr <= 0) {
            Entity ent = ecs_query_iter_get_entity(iter, i);
            ecs_entity_kill(ecs, ent);
        }
    }
}

static f32 ease_out_sine(f32 x) {
    return sinf(x * PI / 2.0f);
}

void hit_system(ECS *ecs, QueryIter iter, void *user_ptr) {
    GameState *state = user_ptr;

    Hit *h = ecs_query_iter_get_field(iter, 0);
    Transform *t = ecs_query_iter_get_field(iter, 1);
    for (u32 i = 0; i < iter.count; i++) {
        f32 life = 0.5f;
        f32 ease = ease_out_sine(h[i].timer/life * PI/2.0f) * 15.0f * state->dt;
        t[i].position.y += ease;
        h[i].timer += state->dt;
        if (h[i].timer >= life) {
            Entity ent = ecs_query_iter_get_entity(iter, i);
            ecs_entity_kill(ecs, ent);
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
        .gravity = -9.82f,
        .grid = grid_new(4096, vec2(5.0f, 5.0f), ALLOCATOR_LIBC),
        .cam = {
            .direction = vec2(1.0f, 1.0f),
            .screen_size = window_get_size(window),
            .zoom = 50.0f,
        },
    };
}

void game_state_free(GameState *state) {
    ecs_free(state->ecs);
    grid_free(&state->grid);
    // Always call 'renderer_free()' before 'window_free()' becaues the former
    // uses OpenGL functions that are no longer available after 'window_free()'
    // has been called.
    renderer_free(state->renderer);
    window_free(state->window);
}

void setup_ecs(GameState *state) {
    state->group = ecs_system_group(state->ecs);

    ecs_register_component(state->ecs, Transform);
    ecs_register_component(state->ecs, Player);
    ecs_register_component(state->ecs, Renderable);
    ecs_register_component(state->ecs, PhysicsBody);
    ecs_register_component(state->ecs, Projectile);
    ecs_register_component(state->ecs, Enemy);
    ecs_register_component(state->ecs, Health);
    ecs_register_component(state->ecs, Hit);
    ecs_register_component(state->ecs, Boss);

    ecs_register_system(state->ecs, player_input_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Player),
                QUERY_FIELDS_END,
            },
        });
    ecs_register_system(state->ecs, player_control_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Player),
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
                [1] = ecs_id(state->ecs, Player),
                QUERY_FIELDS_END,
            },
        });

    ecs_register_system(state->ecs, enemy_ai, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Transform),
                [1] = ecs_id(state->ecs, Enemy),
                QUERY_FIELDS_END,
            },
        });

    ecs_register_system(state->ecs, health_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Health),
                QUERY_FIELDS_END,
            },
        });

    ecs_register_system(state->ecs, hit_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Hit),
                [1] = ecs_id(state->ecs, Transform),
                QUERY_FIELDS_END,
            },
        });


    // Physics should be run last because the spatial partitioning won't be
    // correct otherwise.
    ecs_register_system(state->ecs, physics_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Transform),
                [1] = ecs_id(state->ecs, PhysicsBody),
                QUERY_FIELDS_END,
            },
        });
    ecs_register_system(state->ecs, tile_collision_system, state->group, (QueryDesc) {
            .user_ptr = state,
            .fields = {
                [0] = ecs_id(state->ecs, Transform),
                [1] = ecs_id(state->ecs, PhysicsBody),
                QUERY_FIELDS_END,
            },
        });
}

void setup_world(GameState *state) {
    for (u32 y = 0; y < 5; y++) {
        for (u32 x = 0; x < WORLD_WIDTH; x++) {
            state->tiles[x+y*WORLD_WIDTH] = (Tile) {
                .type = TILE_GROUND,
                    .color = color_rgb_hex(0x212121)
            };
        }
    }
}

void setup_boss(ECS *ecs) {
    Entity boss = ecs_entity(ecs);
    entity_add_component(ecs, boss, Transform, {
            .position = vec2(WORLD_WIDTH/2.0f, WORLD_HEIGHT/2.0f),
            .size = vec2(5.0f, 5.0f),
        });
    entity_add_component(ecs, boss, Renderable, {
            .color = color_rgb_hex(0x4e03fc),
            .texture = TEXTURE_NULL,
        });
    entity_add_component(ecs, boss, PhysicsBody, {
            .gravity_multiplier = 0.0f,
            .is_static = false,
            .collider = true,
        });
    entity_add_component(ecs, boss, Health, {
            .max = 500,
            .curr = 500,
        });
    entity_add_component(ecs, boss, Enemy, {
            .ai = ENEMY_AI_BOSS,
            .target = -1,
            .shoot_delay = 0.25f,
        });
    entity_add_component(ecs, boss, Boss, {});
}

i32 main(void) {
    GameState game_state = game_state_new();
    setup_ecs(&game_state);
    setup_world(&game_state);
    window_set_vsync(game_state.window, false);
    window_set_fullscreen(game_state.window, true);

    Font *font = font_init(str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), game_state.renderer, false, ALLOCATOR_LIBC);

    Entity player = ecs_entity(game_state.ecs);
    entity_add_component(game_state.ecs, player, Transform, {
            .position = vec2(WORLD_WIDTH/2.0f, WORLD_HEIGHT/8.0f),
            .size = vec2(1.0f, 1.0f),
        });
    entity_add_component(game_state.ecs, player, Player, {
            .acceleration = 5.0f,
            .deceleration = 40.0f,
            .max_horizontal_speed = 30.0f,

            .max_fall_speed = -90.0f,
            .max_flight_time = 10.0f,
            .max_vertical_speed = 30.0f,
            .flight_acc = 5.0f,

            .shoot_delay = 1.0f / 5.0f,
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
    entity_add_component(game_state.ecs, player, Health, {
            .max = 100,
            .curr = 100,
        });

    setup_boss(game_state.ecs);

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
        {
            grid_clear(&game_state.grid);
            Query query = ecs_query(game_state.ecs, (QueryDesc) {
                    .fields = {
                    ecs_id(game_state.ecs, Transform),
                    QUERY_FIELDS_END,
                    },
                });
            for (u32 i = 0; i < query.count; i++) {
                QueryIter iter = ecs_query_get_iter(query, i);
                for (u32 j = 0; j < iter.count; j++) {
                    Entity ent = ecs_query_iter_get_entity(iter, j);
                    grid_insert(&game_state.grid, game_state.ecs, ent);
                }
            }
            ecs_query_free(game_state.ecs, query);
        }

        ecs_run_group(game_state.ecs, game_state.group);
        entity_to_entity_collision(&game_state);

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
                        renderer_draw_aabb(game_state.renderer, (AABB) {
                                .position = vec2(x, y),
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
                renderer_draw_aabb(game_state.renderer, (AABB) {
                        .position = t[j].position,
                        .size = t[j].size,
                    }, vec2s(0.0f), r[j].texture, r[j].color);
            }
        }
        ecs_query_free(game_state.ecs, query);

        for (u32 i = 0; i < game_state.debug_draw_i; i++) {
            DebugDraw draw = game_state.debug_draw[i];
            renderer_draw_aabb(game_state.renderer,
                    draw.aabb,
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

        // Health
        query = ecs_query(game_state.ecs, (QueryDesc) {
                .fields = {
                    ecs_id(game_state.ecs, Transform),
                    ecs_id(game_state.ecs, Health),
                    QUERY_FIELDS_END,
                },
            });
        for (u32 i = 0; i < query.count; i++) {
            QueryIter iter = ecs_query_get_iter(query, i);
            Transform *t = ecs_query_iter_get_field(iter, 0);
            Health *h = ecs_query_iter_get_field(iter, 1);
            for (u32 j = 0; j < iter.count; j++) {
                Vec2 half_size = aabb_half_size(*t);
                Vec2 over_entity = t[j].position;
                over_entity.y += half_size.y;

                Vec2 screen_pos = world_to_screen_space(game_state.cam, over_entity);
                screen_pos.y -= 10.0f;

                Vec2 bar_size = vec2(50.0f, 5.0f);

                // Border
                f32 border_padding = 1;
                screen_pos.y += border_padding;
                renderer_draw_aabb(game_state.renderer, (AABB) {
                        .position = screen_pos,
                        .size = vec2_adds(bar_size, border_padding*2.0f),
                    }, vec2(0.0f, 1.0f), TEXTURE_NULL, COLOR_BLACK);
                screen_pos.y -= border_padding;
                // Max health
                renderer_draw_aabb(game_state.renderer, (AABB) {
                        .position = screen_pos,
                        .size = bar_size,
                    }, vec2(0.0f, 1.0f), TEXTURE_NULL, color_rgb_hex(0x1c0806));
                // Current health
                f32 percentage = (f32) h[j].curr / (f32) h[j].max;
                bar_size.x *= percentage;
                renderer_draw_aabb(game_state.renderer, (AABB) {
                        .position = screen_pos,
                        .size = bar_size,
                    }, vec2(0.0f, 1.0f), TEXTURE_NULL, color_rgb_hex(0xff4330));

                // Text
                f32 text_size = 16.0f;
                char health_cstr[32] = {0};
                snprintf(health_cstr, arrlen(health_cstr), "%d/%d", h[j].curr, h[j].max);
                Ivec2 health_text_size = font_measure_string(font, str_cstr(health_cstr), text_size);
                screen_pos.y -= bar_size.y;
                screen_pos.x -= health_text_size.x / 2.0f;
                screen_pos.y -= health_text_size.y;
                renderer_draw_string(game_state.renderer,
                        str_cstr(health_cstr),
                        font,
                        text_size,
                        ivec2(vec2_arg(screen_pos)),
                        COLOR_WHITE);
            }
        }
        ecs_query_free(game_state.ecs, query);

        // :hit_text
        query = ecs_query(game_state.ecs, (QueryDesc) {
                .fields = {
                    ecs_id(game_state.ecs, Transform),
                    ecs_id(game_state.ecs, Hit),
                    QUERY_FIELDS_END,
                },
            });
        for (u32 i = 0; i < query.count; i++) {
            QueryIter iter = ecs_query_get_iter(query, i);
            Transform *t = ecs_query_iter_get_field(iter, 0);
            Hit *h = ecs_query_iter_get_field(iter, 1);
            for (u32 j = 0; j < iter.count; j++) {
                Vec2 screen_pos = world_to_screen_space(game_state.cam, t[j].position);

                // Text
                f32 text_size = 16.0f;
                char hit_str[32] = {0};
                snprintf(hit_str, arrlen(hit_str), "%d", h[j].damage);
                Ivec2 health_text_size = font_measure_string(font, str_cstr(hit_str), text_size);
                screen_pos.x -= health_text_size.x / 2.0f;
                screen_pos.y -= health_text_size.y / 2.0f;
                renderer_draw_string(game_state.renderer,
                        str_cstr(hit_str),
                        font,
                        text_size,
                        ivec2(vec2_arg(screen_pos)),
                        h->color);
            }
        }
        ecs_query_free(game_state.ecs, query);

        renderer_end(game_state.renderer);
        renderer_submit(game_state.renderer);

        if (key_press(game_state.window, KEY_F11)) {
            window_toggle_fullscreen(game_state.window);
        }

        window_poll_event(game_state.window);
        window_swap_buffers(game_state.window);
    }

    game_state_free(&game_state);
    return 0;
}
