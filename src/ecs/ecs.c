#include "ecs.h"
#include "ecs/internal.h"

#include <stdlib.h>

#include "ds.h"
#include "str.h"

ECS *ecs_new(void) {
    ECS *ecs = malloc(sizeof(ECS));
    *ecs = (ECS) {0};

    HashMapDesc component_map_desc = hash_map_desc_default(ecs->component_map);
    component_map_desc.cmp = str_cmp;
    component_map_desc.hash = str_hash;
    hash_map_new(ecs->component_map, component_map_desc);

    Vec(ComponentId) null_archetype = NULL;
    vec_push(ecs->archetypes, archetype_new(ecs, null_archetype));
    vec_free(null_archetype);

    return ecs;
}

void ecs_free(ECS *ecs) {
    hash_map_free(ecs->component_map);

    for (size_t i = 0; i < vec_len(ecs->archetypes); i++) {
        archetype_free(ecs->archetypes[i]);
    }
    vec_free(ecs->archetypes);

    free(ecs);
}

void _ecs_register_component(ECS *ecs, Str component_name, size_t component_size) {
    hash_map_insert(ecs->component_map, component_name, vec_len(ecs->components));
    Component comp = {
        .size = component_size,
    };
    vec_push(ecs->components, comp);
}

Entity ecs_entity(ECS *ecs) {
    uint32_t index = 0;
    uint32_t generation = 0;
    if (vec_len(ecs->entity_free_list) > 0) {
        index = vec_pop(ecs->entity_free_list);
        generation = ecs->entity_generation[index];
    } else {
        index = ecs->entity_curr_index++;
        vec_push(ecs->entity_generation, 0);
    }

    EntityId id = index & (uint64_t) generation << 32;
    ArchetypeColumn column = archetype_column_new(&ecs->archetypes[0]);
    hash_map_insert(ecs->entity_map, index, column);

    return (Entity) {
        .ecs = ecs,
        .id = id,
    };
}

void _entity_add_component(Entity entity, Str component_name, const void *data) {}
