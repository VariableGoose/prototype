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
    entity_add_component(ent, Velocity, {0});
    entity_add_component(ent2, Velocity, {256, 123});

    entity_add_component(ent, Position, {1, 2});
    entity_add_component(ent, Size, {42.0f, 3.1415926f});

    Position *pos = entity_get_component(ent, Position);
    printf("%f, %f\n", pos->x, pos->y);
    Size *size = entity_get_component(ent, Size);
    printf("%f, %f\n", size->x, size->y);

    Velocity *vel = entity_get_component(ent2, Velocity);
    printf("%f, %f\n", vel->x, vel->y);

    ecs_free(ecs);

    return 0;
}
