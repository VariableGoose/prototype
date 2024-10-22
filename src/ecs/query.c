#include "ds.h"
#include "internal.h"

#include <assert.h>
#include <stdio.h>

Query ecs_query(ECS *ecs, QueryDesc desc) {
    size_t field_count = 0;
    Vec(HashSet(Archetype *)) sets = NULL;
    for (; field_count < MAX_QUERY_FIELDS; field_count++) {
        if (desc.fields[field_count] == QUERY_FIELDS_END) {
            break;
        }

        HashSet(Archetype *) set = hash_map_get(ecs->component_archetype_set_map, desc.fields[field_count]);
        if (set == NULL) {
            return (Query) {0};
        }
        vec_push(sets, set);
    }

    if (vec_len(sets) == 0) {
        return (Query) {0};
    } else if (vec_len(sets) == 1) {
        Vec(Archetype *) archetypes = hash_set_to_vec(sets[0]);
        return (Query) {
            .count = vec_len(archetypes),
            ._archetypes = (void **) archetypes,
        };
    }

    HashSet(Archetype *) intersection = sets[0];
    for (size_t i = 1; i < vec_len(sets); i++) {
        HashSet(Archetype *) old_intersection = intersection;
        intersection = hash_set_intersect(intersection, sets[i]);
        // Don't free the first one since that hash set lives inside
        // component_archetype_set_map.
        if (i != 1) {
            hash_set_free(old_intersection);
        }
    }
    vec_free(sets);

    Vec(Archetype *) archetypes = hash_set_to_vec(intersection);
    hash_set_free(intersection);

    return (Query) {
        .count = vec_len(archetypes),
        ._desc = desc,
        ._field_count = field_count,
        ._archetypes = (void **) archetypes,
    };
}

QueryIter ecs_query_get_iter(Query query, size_t i) {
    assert(i < query.count);

    Vec(Archetype *) archetypes = (Archetype **) query._archetypes; 

    size_t count = archetypes[i]->current_index;
    return (QueryIter) {
        .count = count,
        ._i = i,
        ._query = query,
    };
}

void *ecs_query_iter_get_field(QueryIter iter, size_t field) {
    assert(field < iter._query._field_count);

    Archetype *archetype = ((Vec(Archetype *)) iter._query._archetypes)[iter._i];
    size_t row = hash_map_get(archetype->component_lookup, iter._query._desc.fields[field]);

    return archetype->storage[row];
}

void ecs_query_free(Query query) {
    vec_free(query._archetypes);
}
