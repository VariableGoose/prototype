#include <stdio.h>

#include "ecs.h"

typedef struct Vec2 Vec2;
struct Vec2 {
    float x, y;
};

typedef Vec2 Position;
typedef Vec2 Velocity;
typedef Vec2 Size;

void move_system(ECS *ecs, QueryIter iter) {
    (void) ecs;

    Position *pos = ecs_query_iter_get_field(iter, 0);
    Position *vel = ecs_query_iter_get_field(iter, 1);
    for (size_t i = 0; i < iter.count; i++) {
        pos[i].x += vel[i].x;
        pos[i].y += vel[i].y;

        printf("Moved entity to (%f, %f)\n", pos[i].x, pos[i].y);
    }
}

int main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Position);
    ecs_register_component(ecs, Velocity);
    ecs_register_component(ecs, Size);

    Entity ent = ecs_entity(ecs);
    Entity ent2 = ecs_entity(ecs);
    entity_add_component(ecs, ent, Position, {1, 2});
    entity_add_component(ecs, ent, Velocity, {1, 1});

    entity_add_component(ecs, ent2, Position, {42.0f, 3.1415926f});
    entity_add_component(ecs, ent2, Velocity, {256, 123});

    Query query = ecs_query(ecs, (QueryDesc) {
            .fields = {
                [0] = ecs_id(ecs, Position),
                [1] = ecs_id(ecs, Velocity),
                QUERY_FIELDS_END,
            },
        });
    printf("count: %zu\n", query.count);

    // Iterate over all archetypes.
    for (size_t i = 0; i < query.count; i++) {
        QueryIter iter = ecs_query_get_iter(query, i);
        printf("iter count: %zu\n", iter.count);
        // Iterate over all entities within the current archetype.
        move_system(ecs, iter);
    }
    ecs_query_free(query);

    ecs_free(ecs);

    return 0;
}
