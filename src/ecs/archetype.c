#include "ds.h"
#include "ecs/internal.h"

Archetype archetype_new(const ECS *ecs, Vec(ComponentId) types) {
    Vec(ComponentId) type_clone = NULL;
    vec_insert_arr(type_clone, 0, types, vec_len(types));

    size_t sum = 0;
    for (size_t i = 0; i < vec_len(types); i++) {
        sum += ecs->components[types[i]].size;
    }
    void *storage = NULL;
    _vec_create(&storage, sum);

    return (Archetype) {
        .types = type_clone,
        .storage = storage,
    };
}

void archetype_free(Archetype archetype) {
    vec_free(archetype.storage);
    vec_free(archetype.types);
}

ArchetypeColumn archetype_column_new(Archetype *archetype) {
    size_t index = vec_len(archetype->storage);
    vec_push(archetype->storage, NULL);

    return (ArchetypeColumn) {
        .archetype = archetype,
        .index = index,
    };
}
