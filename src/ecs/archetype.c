#include "ds.h"
#include "internal.h"

#include <stdio.h>

Archetype archetype_new(const ECS *ecs, Type type) {
    return (Archetype) {
        .type = type_clone(type),
    };
}

void archetype_free(Archetype archetype) {
    vec_free(archetype.storage);
    type_free(archetype.type);
    hash_map_free(archetype.edge_map);
}

ArchetypeColumn archetype_column_new(Archetype *archetype) {
    size_t index = vec_len(archetype->storage);
    vec_push(archetype->storage, NULL);

    return (ArchetypeColumn) {
        .archetype = archetype,
        .index = index,
    };
}

ArchetypeColumn archetype_move(Archetype *current, Archetype *new, size_t index) {
    ArchetypeColumn column = archetype_column_new(new);

    // TODO: Move component data from current archetype to the new one.

    return column;
}
