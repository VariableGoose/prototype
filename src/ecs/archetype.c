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
        hash_map_insert(archetype->component_lookup, type[i], i);
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
    hash_map_free(archetype->component_lookup);
    free(archetype);
}

ArchetypeColumn archetype_add_entity(Archetype *archetype, EntityId entity) {
    size_t index = archetype->current_index++;
    hash_map_insert(archetype->entity_lookup, index, entity);

    return (ArchetypeColumn) {
        .archetype = archetype,
        .index = index,
    };
}

// static void create_edges(Archetype *root, Archetype *archetype) {
//     // If the root has more than one more component than archetype we don't need to check it
//     // because we take single component steps in the graph.
//     if (type_len(root->type) > type_len(archetype->type) + 1) {
//         return;
//     }
//
//     ComponentId diff;
//
//     // Create add link
//     if (type_len(root->type) == type_len(archetype->type) - 1 &&
//         type_is_proper_subset(root->type, archetype->type, &diff)) {
//         ArchetypeEdge edge = {
//             .add = archetype,
//             .remove = root,
//         };
//         hash_map_insert(root->edge_map, diff, edge);
//         hash_map_insert(archetype->edge_map, diff, edge);
//     }
//
//     // Create remove link
//     if (type_len(root->type) == type_len(archetype->type) + 1 &&
//         type_is_proper_subset(archetype->type, root->type, &diff)) {
//         ArchetypeEdge edge = {
//             .add = root,
//             .remove = archetype,
//         };
//         hash_map_insert(root->edge_map, diff, edge);
//         hash_map_insert(archetype->edge_map, diff, edge);
//     }
//
//     for (HashMapIter i = hash_map_iter_new(root->edge_map);
//         hash_map_iter_valid(root->edge_map, i);
//         i = hash_map_iter_next(root->edge_map, i)) {
//         if (root->edge_map[i].value.add == root) {
//             continue;
//         }
//         create_edges(root->edge_map[i].value.add, archetype);
//     }
// }

void archetype_move_entity_right(ECS *ecs, Archetype *left, const void *component_data, ComponentId component_id, size_t left_column) {
    ArchetypeEdge edge = hash_map_get(left->edge_map, component_id);
    Archetype *right = edge.add;
    if (edge.add == NULL) {
        Type right_type = type_clone(left->type);
        type_add(right_type, component_id);
        right = hash_map_get(ecs->archetype_map, right_type);
        if (right == NULL) {
            right = archetype_new(ecs, right_type);
            hash_map_insert(ecs->archetype_map, right->type, right);
        }
        type_free(right_type);

        ArchetypeEdge edge = {
            .add = right,
            .remove = left,
        };
        hash_map_insert(left->edge_map, component_id, edge);
        hash_map_insert(right->edge_map, component_id, edge);

        // create_edges(ecs->archetypes[0], new_archetype);
    }

    // Swap place of the last entity and the one being moved.
    EntityId last_left_entity = hash_map_get(left->entity_lookup, left->current_index-1);
    EntityId entity_to_move = hash_map_get(left->entity_lookup, left_column);
    ArchetypeColumn column = {
        .archetype = left,
        .index = left_column,
    };
    hash_map_set(ecs->entity_map, last_left_entity, column);
    hash_map_set(left->entity_lookup, left_column, last_left_entity);
    left->current_index--;

    // Place the entity in the right archetype.
    size_t right_column = right->current_index++;
    hash_map_insert(right->entity_lookup, right_column, entity_to_move);
    column = (ArchetypeColumn) {
        .archetype = right,
        .index = right_column,
    };
    hash_map_set(ecs->entity_map, entity_to_move, column);

    // Add the entity to the storage of the right archetype.
    for (size_t i = 0; i < type_len(left->type); i++) {
        ComponentId comp = left->type[i];
        size_t index = hash_map_get(right->component_lookup, comp);
        size_t component_size = ecs->components[comp].size;
        _vec_insert_fast(&right->storage[index], vec_len(right->storage[index]), left->storage[i] + component_size*left_column);
    }

    // Populate the empty row with data of the component being added.
    size_t index = hash_map_get(right->component_lookup, component_id);
    _vec_insert_fast(&right->storage[index], vec_len(right->storage[index]), component_data);

    // Remove the moved entity column and place the last entity column into the
    // now empty column.
    for (size_t i = 0; i < type_len(left->type); i++) {
        _vec_remove_fast(&left->storage[i], left_column, NULL);
    }
}

void archetype_move_entity_left(ECS *ecs, Archetype *right, ComponentId component_id, size_t right_column) {
    ArchetypeEdge edge = hash_map_get(right->edge_map, component_id);
    Archetype *left = edge.remove;
    if (edge.add == NULL) {
        Type left_type = type_clone(right->type);
        type_remove(left_type, component_id);
        left = hash_map_get(ecs->archetype_map, left_type);
        if (left == NULL) {
            left = archetype_new(ecs, left_type);
            hash_map_insert(ecs->archetype_map, left->type, left);
        }
        type_free(left_type);

        ArchetypeEdge edge = {
            .add = right,
            .remove = left,
        };
        hash_map_insert(left->edge_map, component_id, edge);
        hash_map_insert(right->edge_map, component_id, edge);

        // create_edges(ecs->archetypes[0], new_archetype);
    }

    // Swap place of the last entity and the one being moved.
    EntityId last_right_entity = hash_map_get(right->entity_lookup, right->current_index-1);
    EntityId entity_to_move = hash_map_get(right->entity_lookup, right_column);
    ArchetypeColumn column = {
        .archetype = right,
        .index = right_column,
    };
    hash_map_set(ecs->entity_map, last_right_entity, column);
    hash_map_set(right->entity_lookup, right_column, last_right_entity);
    right->current_index--;

    // Place the entity in the right archetype.
    size_t left_column = left->current_index++;
    hash_map_insert(left->entity_lookup, left_column, entity_to_move);
    column = (ArchetypeColumn) {
        .archetype = left,
        .index = left_column,
    };
    hash_map_set(ecs->entity_map, entity_to_move, column);

    // Add the entity to the storage of the right archetype.
    for (size_t i = 0; i < type_len(left->type); i++) {
        ComponentId comp = left->type[i];
        size_t index = hash_map_get(right->component_lookup, comp);
        size_t component_size = ecs->components[comp].size;
        _vec_insert_fast(&right->storage[index], vec_len(right->storage[index]), left->storage[i] + component_size*left_column);
    }

    // Remove the moved entity column and place the last entity column into the
    // now empty column.
    for (size_t i = 0; i < type_len(right->type); i++) {
        _vec_remove_fast(&right->storage[i], right_column, NULL);
    }
}
