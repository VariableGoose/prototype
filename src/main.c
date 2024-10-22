#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

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

        // printf("Moved entity to (%f, %f)\n", pos[i].x, pos[i].y);
    }
}

int main(void) {
    ECS *ecs = ecs_new();
    ecs_register_component(ecs, Position);
    ecs_register_component(ecs, Velocity);
    ecs_register_component(ecs, Size);

    SystemGroup group = ecs_system_group(ecs);

    ecs_register_system(ecs, move_system, group, (QueryDesc) {
            .fields = {
                [0] = ecs_id(ecs, Position),
                [1] = ecs_id(ecs, Velocity),
            },
        });

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double ms = (float) ts.tv_sec*1e3 + ts.tv_nsec/1e6;

    for (int i = 0; i < 8; i++) {
        Entity ent = ecs_entity(ecs);
        printf("%lu\n", ent);
        entity_add_component(ecs, ent, Position, {1, 1});
        entity_add_component(ecs, ent, Velocity, {i, 2*i});
        ecs_entity_kill(ecs, ent);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ms = (float) ts.tv_sec*1e3 + ts.tv_nsec/1e6 - ms;
    printf("diff: %fms\n", ms);

    double last = 0.0f;

    int fps = 0;
    double fps_timer = 0.0f;
    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double curr = ts.tv_sec + ts.tv_nsec/1e9;
        double dt = curr - last;
        last = curr;

        fps_timer += dt;
        fps++;
        if (fps_timer >= 1.0f) {
            printf("fps: %u\n", fps);
            fps = 0;
            fps_timer = 0.0f;
        }

        ecs_run(ecs, group);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ms = (float) ts.tv_sec*1e3 + ts.tv_nsec/1e6 - ms;
    printf("diff: %fms\n", ms);

    ecs_free(ecs);

    return 0;
}
