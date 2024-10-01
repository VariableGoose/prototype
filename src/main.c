#include <stdio.h>

#include "ds.h"

#include "ecs.h"
#include "str.h"

typedef struct Vec2 Vec2;
struct Vec2 {
    float x, y;
};

typedef Vec2 Position;
typedef Vec2 Velocity;

int main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Position);
    ecs_register_component(ecs, Velocity);

    Entity ent = ecs_entity(ecs);
    entity_add_component(ent, Position, {0});
    entity_add_component(ent, Velocity, {0});

    ent = ecs_entity(ecs);
    entity_add_component(ent, Position, {0});
    entity_add_component(ent, Velocity, {0});

    ecs_free(ecs);

    return 0;
}
