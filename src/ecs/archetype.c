#include "ds.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

Archetype *archetype_new(const ECS *ecs, Type type) {
    Archetype *archetype = malloc(sizeof(Archetype));
    *archetype = (Archetype) {
        .type = type_clone(type),
    };
    return archetype;
}

void archetype_free(Archetype *archetype) {
    vec_free(archetype->storage);
    type_free(archetype->type);
    hash_map_free(archetype->edge_map);
    free(archetype);
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
