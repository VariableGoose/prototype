#include "ds.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

Archetype *archetype_new(const ECS *ecs, Type type) {
    Archetype *archetype = malloc(sizeof(Archetype));
    *archetype = (Archetype) {
        .type = type_clone(type),
    };

    for (size_t i = 0; i < type_len(type); i++) {
        vec_push(archetype->storage, vec_new(ecs->components[type[i]].size));
        hash_map_insert(archetype->component_index_map, type[i], i);
    }

    return archetype;
}

void archetype_free(Archetype *archetype) {
    for (size_t i = 0; i < vec_len(archetype->storage); i++) {
        vec_free(archetype->storage[i]);
    }
    vec_free(archetype->storage);
    type_free(archetype->type);
    hash_map_free(archetype->edge_map);
    hash_map_free(archetype->component_index_map);
    free(archetype);
}

ArchetypeColumn archetype_column_new(Archetype *archetype) {
    size_t index = archetype->current_index++;

    for (size_t i = 0; i < vec_len(archetype->storage); i++) {
        vec_push(archetype->storage[i], NULL);
    }

    return (ArchetypeColumn) {
        .archetype = archetype,
        .index = index,
    };
}

ArchetypeColumn archetype_move(const ECS *ecs, Archetype *current, Archetype *new, size_t index) {
    ArchetypeColumn column = archetype_column_new(new);

    // Copy component data from old to new archetype.
    for (size_t i = 0; i < type_len(current->type); i++) {
        ComponentId component = current->type[i];
        size_t *idx = hash_map_getp(new->component_index_map, component);
        if (idx == NULL) {
            continue;
        }
        void *old_storage_location = current->storage[i] + ecs->components[component].size*index;
        void *new_storage_location = new->storage[*idx] + ecs->components[component].size*column.index;
        memcpy(new_storage_location, old_storage_location, ecs->components[component].size);
    }

    return column;
}
