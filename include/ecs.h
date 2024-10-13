#pragma once

#include <stdint.h>

#include "str.h"

typedef struct ECS ECS;

typedef uint64_t Entity;

extern ECS *ecs_new(void);
extern void ecs_free(ECS *ecs);

#define ecs_register_component(ecs, component) \
    _ecs_register_component(ecs, str_lit(#component), sizeof(component))
extern void _ecs_register_component(ECS *ecs, Str component_name,
                                    size_t component_size);

// -- Entity -------------------------------------------------------------------
extern Entity ecs_entity(ECS *ecs);

#define entity_add_component(ecs, entity, component, ...) \
    _entity_add_component(ecs, entity, str_lit(#component), &(component)__VA_ARGS__)
extern void _entity_add_component(ECS *ecs, Entity entity, Str component_name,
        const void *data);

#define entity_remove_component(ecs, entity, component) \
    _entity_remove_component(ecs, entity, str_lit(#component))
extern void _entity_remove_component(ECS *ecs, Entity entity, Str component_name);

#define entity_get_component(ecs, entity, component) \
    _entity_get_component(ecs, entity, str_lit(#component))
extern void *_entity_get_component(ECS *ecs, Entity entity, Str component_name);
