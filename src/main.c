#include <stdio.h>

#include "ecs.h"

typedef struct Vec2 Vec2;
struct Vec2 {
    float x, y;
};

typedef Vec2 Position;
typedef Vec2 Velocity;
typedef Vec2 Size;

int main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Position);
    ecs_register_component(ecs, Velocity);
    ecs_register_component(ecs, Size);


    Entity ent = ecs_entity(ecs);
    Entity ent2 = ecs_entity(ecs);
    entity_add_component(ecs, ent, Velocity, {0});
    entity_add_component(ecs, ent2, Velocity, {256, 123});

    // Entity a = ecs_entity(ecs);
    // entity_add_entity(ecs, ent, a);

    entity_add_component(ecs, ent, Position, {1, 2});
    entity_add_component(ecs, ent, Size, {42.0f, 3.1415926f});

    entity_remove_component(ecs, ent, Velocity);

    Position *pos = entity_get_component(ecs, ent, Position);
    printf("%f, %f\n", pos->x, pos->y);
    Size *size = entity_get_component(ecs, ent, Size);
    printf("%f, %f\n", size->x, size->y);
    Velocity *vel = entity_get_component(ecs, ent, Velocity);
    printf("%p\n", vel);

    Velocity *vel2 = entity_get_component(ecs, ent2, Velocity);
    printf("%f, %f\n", vel2->x, vel2->y);

    ecs_free(ecs);

    return 0;
}
