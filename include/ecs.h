#pragma once

#include <stdint.h>

#include "str.h"

typedef struct ECS ECS;

typedef struct Entity Entity;
struct Entity {
    const ECS *ecs;
    uint64_t id;
};

extern ECS *ecs_new(void);
extern void ecs_free(ECS *ecs);

extern Entity ecs_entity(ECS *ecs);

#define entity_add_component(entity, component, ...) \
    _entity_add_component(entity, str_lit(#component), &(component) __VA_ARGS__)
extern void _entity_add_component(Entity entity, Str component_name, const void *data);

#define ecs_register_component(ecs, component) \
    _ecs_register_component(ecs, str_lit(#component), sizeof(component))
extern void _ecs_register_component(ECS *ecs, Str component_name, size_t component_size);
