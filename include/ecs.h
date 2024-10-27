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

#define ecs_id(ecs, component) _ecs_id(ecs, str_lit(#component))
extern Entity _ecs_id(ECS *ecs, Str component_name);

// -- Entity -------------------------------------------------------------------
extern Entity ecs_entity(ECS *ecs);
extern void ecs_entity_kill(ECS *ecs, Entity entity);

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

// extern void entity_add_entity(ECS *ecs, Entity self, Entity other);
// extern void entity_remove_entity(ECS *ecs, Entity self, Entity other);

// -- Query --------------------------------------------------------------------
#define MAX_QUERY_FIELDS 128
static const Entity QUERY_FIELDS_END = -1;

typedef struct QueryDesc QueryDesc;
struct QueryDesc {
    Entity fields[MAX_QUERY_FIELDS];
    void *user_ptr;
};

typedef struct Query Query;
struct Query {
    size_t count;

    QueryDesc _desc;
    size_t _field_count;
    void **_archetypes;
};

typedef struct QueryIter QueryIter;
struct QueryIter {
    size_t count;

    Query _query;
    size_t _i;
};

extern Query ecs_query(ECS *ecs, QueryDesc desc);
extern QueryIter ecs_query_get_iter(Query query, size_t i);
extern void *ecs_query_iter_get_field(QueryIter iter, size_t field);
extern void ecs_query_free(Query query);

// -- System -------------------------------------------------------------------
typedef size_t SystemGroup;
typedef void (*System)(ECS *ecs, QueryIter iter, void *user_ptr);

extern SystemGroup ecs_system_group(ECS *ecs);
extern void ecs_register_system(ECS *ecs, System system, SystemGroup group, QueryDesc desc);
extern void ecs_run_group(ECS *ecs, SystemGroup group);
extern void ecs_run_system(ECS *ecs, System system, QueryDesc desc);
