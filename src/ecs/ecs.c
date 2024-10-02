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
    component_map_desc.cmp = type_cmp;
    component_map_desc.hash = type_hash;
    hash_map_new(ecs->archetype_map, archetype_map_desc);

    Type null_archetype = NULL;
    vec_push(ecs->archetypes, archetype_new(ecs, null_archetype));
    hash_map_insert(ecs->archetype_map, null_archetype, &ecs->archetypes[0]);
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
    hash_map_free(ecs->archetype_map);

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

static void create_edges(Archetype *root, Archetype *archetype) {
    // If the root has more than one more component than archetype we don't need to check it
    // because we take single component steps in the graph.
    if (type_len(root->type) > type_len(archetype->type) + 1) {
        return;
    }

    // TODO: Add edges between the archetypes.

    // Create add link
    if (type_len(root->type) == type_len(archetype->type) - 1 &&
        type_is_proper_subset(root->type, archetype->type)) {
        printf("Add from root\n");
    }

    // Create remove link
    if (type_len(root->type) == type_len(archetype->type) + 1 &&
        type_is_proper_subset(archetype->type, root->type)) {
        printf("Remove from root\n");
    }

    for (HashMapIter i = hash_map_iter_new(root->edge_map);
        hash_map_iter_valid(root->edge_map, i);
        i = hash_map_iter_next(root->edge_map, i)) {
        create_edges(root->edge_map[i].value.add, archetype);
    }
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
    Archetype *new_archetype = NULL;
    if (edge.add == NULL) {
        Type new_type = type_clone(current_archetype->type);
        type_add(new_type, component_id);
        vec_push(ecs->archetypes, archetype_new(ecs, new_type));
        vec_free(new_type);
        new_archetype = &vec_last(ecs->archetypes);

        printf("No edge\n");

        create_edges(&ecs->archetypes[0], new_archetype);
    } else {
        new_archetype = edge.add;
        printf("Edge\n");
    }

    *column = archetype_move(current_archetype, new_archetype, column->index);
}
