#include "ecs.h"
#include "core.h"
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
    ComponentId null_component = -1;
    component_map_desc.zero_value = &null_component;
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

static void _ecs_internal_entity_spawn(ECS *ecs, Entity id) {
    ArchetypeColumn column = archetype_add_entity(ecs->root_archetype, id);
    hash_map_insert(ecs->entity_map, id, column);
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

    if (ecs->active_queries > 0) {
        vec_push(ecs->command_queue, ((Command) {
                .type = COMMAND_ENTITY_SPAWN,
                .entity = id,
            }));
    } else {
        _ecs_internal_entity_spawn(ecs, id);
    }

    return id;
}

static void _ecs_internal_kill(ECS *ecs, Entity entity) {
    if (!entity_alive(ecs, entity)) {
        return;
    }

    uint32_t index = entity;

    ecs->entity_generation[index]++;
    vec_push(ecs->entity_free_list, index);

    HashMapIter iter = hash_map_remove(ecs->entity_map, entity);
    ArchetypeColumn column = ecs->entity_map[iter].value;
    archetype_remove_entity(ecs, column.archetype, column.index);
}

void ecs_entity_kill(ECS *ecs, Entity entity) {
    if (!entity_alive(ecs, entity)) {
        return;
    }

    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    if (ecs->active_queries > 0) {
        vec_push(ecs->command_queue, ((Command) {
                .type = COMMAND_ENTITY_KILL,
                .entity = entity,
            }));
    } else {
        _ecs_internal_kill(ecs, entity);
    }
}

static void _entity_internal_add_component(ECS *ecs, Entity entity, ComponentId component_id, const void *data) {
    ArchetypeColumn column = hash_map_get(ecs->entity_map, entity);
    Archetype *left_archetype = column.archetype;

    archetype_move_entity_right(ecs, left_archetype, data, component_id, column.index);
}

void _entity_add_component(ECS *ecs, Entity entity, Str component_name, const void *data) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    ComponentId component_id = hash_map_get(ecs->component_map, component_name);
    assert(component_id != (ComponentId) -1 && "Add non-existent component.");

    if (ecs->active_queries > 0) {
        u64 component_size = ecs->components[component_id].size;
        void *data_copy = malloc(component_size);
        memcpy(data_copy, data, component_size);
        vec_push(ecs->command_queue, ((Command) {
                .type = COMMAND_ENTITY_COMPONENT_ADD,
                .entity = entity,
                .component_id = component_id,
                .data = data_copy,
            }));
    } else {
        _entity_internal_add_component(ecs, entity, component_id, data);
    }
}

static void _entity_internal_remove_component(ECS *ecs, Entity entity, ComponentId component_id) {
    ArchetypeColumn *column = hash_map_getp(ecs->entity_map, entity);
    Archetype *right_archetype = column->archetype;

    archetype_move_entity_left(ecs, right_archetype, component_id, column->index);
}

void _entity_remove_component(ECS *ecs, Entity entity, Str component_name) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    ComponentId component_id = hash_map_get(ecs->component_map, component_name);
    assert(component_id != (ComponentId) -1 && "Remove non-existent component.");

    if (ecs->active_queries > 0) {
        vec_push(ecs->command_queue, ((Command) {
                .type = COMMAND_ENTITY_COMPONENT_REMOVE,
                .entity = entity,
                .component_id = component_id,
            }));
    } else {
        _entity_internal_remove_component(ecs, entity, component_id);
    }
}

void *_entity_get_component(ECS *ecs, Entity entity, Str component_name) {
    uint32_t index = entity;
    uint32_t generation = entity >> 32;
    assert(ecs->entity_generation[index] == generation);

    ArchetypeColumn *column = hash_map_getp(ecs->entity_map, entity);
    if (column == NULL) {
        log_error("Column retrieved is null: %zu, %u, %u", entity, (u32) entity, (u32) (entity >> 32));
    }
    ComponentId component_id = hash_map_get(ecs->component_map, component_name);
    assert(component_id != (ComponentId) -1 && "Get non-existent component.");
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
    ecs_query_free(ecs, query);
}

void _ecs_process_command_queue(ECS *ecs) {
    for (u32 i = 0; i < vec_len(ecs->command_queue); i++) {
        Command cmd = ecs->command_queue[i];
        switch (cmd.type) {
            case COMMAND_ENTITY_SPAWN:
                _ecs_internal_entity_spawn(ecs, cmd.entity);
                 break;
            case COMMAND_ENTITY_KILL:
                _ecs_internal_kill(ecs, cmd.entity);
                 break;
            case COMMAND_ENTITY_COMPONENT_ADD:
                _entity_internal_add_component(ecs, cmd.entity, cmd.component_id, cmd.data);
                free(cmd.data);
                 break;
            case COMMAND_ENTITY_COMPONENT_REMOVE:
                _entity_internal_remove_component(ecs, cmd.entity, cmd.component_id);
                 break;
        }
    }
    vec_free(ecs->command_queue);
}
