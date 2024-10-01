#include "ecs.h"
#include "ecs/internal.h"

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

    Vec(ComponentId) null_archetype = NULL;
    vec_push(ecs->archetypes, archetype_new(ecs, null_archetype));
    vec_free(null_archetype);

    return ecs;
}

void ecs_free(ECS *ecs) {
    hash_map_free(ecs->component_map);
    vec_free(ecs->components);

    for (size_t i = 0; i < vec_len(ecs->archetypes); i++) {
        archetype_free(ecs->archetypes[i]);
    }
    vec_free(ecs->archetypes);

    hash_map_free(ecs->entity_map);
    vec_free(ecs->entity_generation);
    vec_free(ecs->entity_free_list);

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

    EntityId id = index | (uint64_t) generation << 32;
    ArchetypeColumn column = archetype_column_new(&ecs->archetypes[0]);
    hash_map_insert(ecs->entity_map, index, column);

    return (Entity) {
        .ecs = ecs,
        .id = id,
    };
}

void _entity_add_component(Entity entity, Str component_name, const void *data) {
    ECS *ecs = entity.ecs;
    uint32_t index = entity.id;
    uint32_t generation = entity.id >> 32;
    assert(ecs->entity_generation[index] == generation);

    ArchetypeColumn *column = hash_map_getp(ecs->entity_map, entity.id);
    Archetype *current_archetype = column->archetype;
    ComponentId component_id = hash_map_get(ecs->component_map, component_name);

    ArchetypeEdge edge = hash_map_get(current_archetype->edge_map, component_id);
    if (edge.add == NULL) {
        Vec(ComponentId) type_add = NULL;
        vec_insert_arr(type_add, 0, current_archetype->types, vec_len(current_archetype->types));
        vec_push(type_add, component_id);
        vec_push(ecs->archetypes, archetype_new(ecs, type_add));
        vec_free(type_add);
        Archetype *new_archetype = &vec_last(ecs->archetypes);

        edge = (ArchetypeEdge) {
            .add = new_archetype,
            .remove = current_archetype,
        };
        hash_map_insert(current_archetype->edge_map, component_id, edge);

        printf("No edge\n");
    } else {
        printf("Edge\n");
    }

    Archetype *new_archetype = edge.add;
    *column = archetype_move(current_archetype, new_archetype, column->index);
}
