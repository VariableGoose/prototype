#include "ecs.h"
#include "internal.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "ds.h"
#include "str.h"

ECS *ecs_new(void) {
    ECS *ecs = malloc(sizeof(ECS));
    *ecs = (ECS) {0};

    HashMapDesc component_map_desc = hash_map_desc_default(ecs->component_map);
    component_map_desc.cmp = str_cmp;
    component_map_desc.hash = str_hash;
    hash_map_new(ecs->component_map, component_map_desc);

    HashMapDesc archetype_map_desc = hash_map_desc_default(ecs->archetype_map);
    archetype_map_desc.cmp = type_cmp;
    archetype_map_desc.hash = type_hash;
    hash_map_new(ecs->archetype_map, archetype_map_desc);

    ecs->root_archetype = archetype_new(ecs, NULL);
    hash_map_insert(ecs->archetype_map, NULL, ecs->root_archetype);

    return ecs;
}

void ecs_free(ECS *ecs) {
    hash_map_free(ecs->component_map);
    vec_free(ecs->components);

    for (size_t i = hash_map_iter_new(ecs->archetype_map);
            hash_map_iter_valid(ecs->archetype_map, i);
            i = hash_map_iter_next(ecs->archetype_map, i)) {
        archetype_free(ecs->archetype_map[i].value);
    }
    hash_map_free(ecs->archetype_map);

    hash_map_free(ecs->entity_map);
    vec_free(ecs->entity_generation);
    vec_free(ecs->entity_free_list);

    for (size_t i = hash_map_iter_new(ecs->component_archetype_set_map);
            hash_map_iter_valid(ecs->component_archetype_set_map, i);
            i = hash_map_iter_next(ecs->component_archetype_set_map, i)) {
        hash_set_free(ecs->component_archetype_set_map[i].value);
    }
    hash_map_free(ecs->component_archetype_set_map);

    for (size_t i = 0; i < vec_len(ecs->systems); i++) {
        vec_free(ecs->systems[i]);
    }

    free(ecs);
}

void _ecs_register_component(ECS *ecs, Str component_name, size_t component_size) {
    hash_map_insert(ecs->component_map, component_name, vec_len(ecs->components));
    Component comp = {
        .size = component_size,
    };
    vec_push(ecs->components, comp);
}

Entity _ecs_id(ECS *ecs, Str component_name) {
    return hash_map_get(ecs->component_map, component_name);
}

Entity ecs_entity(ECS *ecs) {
    uint32_t index = 0;
    uint32_t generation = 0;
    if (vec_len(ecs->entity_free_list) > 0) {
        index = vec_pop(ecs->entity_free_list);
        generation = ecs->entity_generation[index];
    } else {
        index = ecs->entity_current_id++;
        vec_push(ecs->entity_generation, 0);
    }

    Entity id = index | (uint64_t) generation << 32;
    ArchetypeColumn column = archetype_add_entity(ecs->root_archetype, id);
    hash_map_insert(ecs->entity_map, id, column);

    return id;
}

void ecs_entity_kill(ECS *ecs, Entity entity) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    ecs->entity_generation[index]++;
    vec_push(ecs->entity_free_list, index);

    HashMapIter iter = hash_map_remove(ecs->entity_map, entity);
    ArchetypeColumn column = ecs->entity_map[iter].value;
    archetype_remove_entity(ecs, column.archetype, column.index);
}

void _entity_add_component(ECS *ecs, Entity entity, Str component_name, const void *data) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    ArchetypeColumn column = hash_map_get(ecs->entity_map, entity);
    Archetype *left_archetype = column.archetype;
    ComponentId component_id = hash_map_get(ecs->component_map, component_name);

    archetype_move_entity_right(ecs, left_archetype, data, component_id, column.index);
}

void _entity_remove_component(ECS *ecs, Entity entity, Str component_name) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    ArchetypeColumn *column = hash_map_getp(ecs->entity_map, entity);
    Archetype *right_archetype = column->archetype;
    ComponentId component_id = hash_map_get(ecs->component_map, component_name);

    archetype_move_entity_left(ecs, right_archetype, component_id, column->index);
}

void *_entity_get_component(ECS *ecs, Entity entity, Str component_name) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    ArchetypeColumn *column = hash_map_getp(ecs->entity_map, entity);
    ComponentId component_id = hash_map_get(ecs->component_map, component_name);
    size_t *row = hash_map_getp(column->archetype->component_lookup, component_id);
    // Archetype doesn't have component.
    if (row == NULL) {
        return NULL;
    }
    size_t component_size = ecs->components[component_id].size;
    return column->archetype->storage[*row] + component_size*column->index;
}

b8 entity_alive(ECS *ecs, Entity entity) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    return index < ecs->entity_current_id && ecs->entity_generation[index] == generation;
}

SystemGroup ecs_system_group(ECS *ecs) {
    SystemGroup group = vec_len(ecs->systems);
    vec_push(ecs->systems, NULL);
    return group;
}

void ecs_register_system(ECS *ecs, System system, SystemGroup group, QueryDesc desc) {
    assert(group < vec_len(ecs->systems));

    vec_push(ecs->systems[group], ((InternalSystem) {
            .func = system,
            .desc = desc,
        }));
}

void ecs_run_group(ECS *ecs, SystemGroup group) {
    assert(group < vec_len(ecs->systems));

    for (size_t i = 0; i < vec_len(ecs->systems[group]); i++) {
        InternalSystem system = ecs->systems[group][i];
        ecs_run_system(ecs, system.func, system.desc);
    }
}

void ecs_run_system(ECS *ecs, System system, QueryDesc desc) {
    Query query = ecs_query(ecs, desc);
    for (size_t i = 0; i < query.count; i++) {
        QueryIter iter = ecs_query_get_iter(query, i);
        system(ecs, iter, desc.user_ptr);
    }
}
