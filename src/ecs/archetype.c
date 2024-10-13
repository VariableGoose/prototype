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

ArchetypeColumn archetype_add_entity(Archetype *archetype, Entity entity) {
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

static void archetype_move_entity(ECS *ecs, Archetype *current, Archetype *next, size_t current_column) {
    // Swap place of the last entity and the one being moved.
    Entity last_current_entity = hash_map_get(current->entity_lookup, current->current_index-1);
    Entity entity_to_move = hash_map_get(current->entity_lookup, current_column);
    ArchetypeColumn column = {
        .archetype = current,
        .index = current_column,
    };
    hash_map_set(ecs->entity_map, last_current_entity, column);
    hash_map_set(current->entity_lookup, current_column, last_current_entity);
    current->current_index--;

    // Place the entity in the next archetype.
    size_t next_column = next->current_index++;
    hash_map_insert(next->entity_lookup, next_column, entity_to_move);
    column = (ArchetypeColumn) {
        .archetype = next,
        .index = next_column,
    };
    hash_map_set(ecs->entity_map, entity_to_move, column);

    // Add the entity to the storage of the right archetype.
    Archetype *smaller_archetype = type_len(current->type) > type_len(next->type) ? next : current;
    Archetype *bigger_archetype = type_len(current->type) > type_len(next->type) ? current : next;
    for (size_t i = 0; i < type_len(smaller_archetype->type); i++) {
        ComponentId comp = smaller_archetype->type[i];
        size_t index = hash_map_get(bigger_archetype->component_lookup, comp);
        size_t component_size = ecs->components[comp].size;
        _vec_insert_fast(&bigger_archetype->storage[index], vec_len(bigger_archetype->storage[index]), smaller_archetype->storage[i] + component_size*next_column);
    }

    // Remove the moved entity column and place the last entity column into the
    // now empty column.
    for (size_t i = 0; i < type_len(current->type); i++) {
        _vec_remove_fast(&current->storage[i], current_column, NULL);
    }
}

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

    archetype_move_entity(ecs, left, right, left_column);

    // Populate the empty row with data of the component being added.
    size_t index = hash_map_get(right->component_lookup, component_id);
    _vec_insert_fast(&right->storage[index], vec_len(right->storage[index]), component_data);
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

    archetype_move_entity(ecs, right, left, right_column);
}
