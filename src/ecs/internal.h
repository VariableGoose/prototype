#pragma once

#include "ecs.h"
#include "ds.h"

typedef size_t ComponentId;

// -- Type ---------------------------------------------------------------------
// A set of unique component IDs.
typedef Vec(ComponentId) Type;

extern void type_add(Type type, ComponentId component);
extern void type_remove(Type type, ComponentId component);
extern Type type_clone(const Type type);
extern size_t type_hash(const void *data, size_t size);
extern int type_cmp(const void *a, const void *b, size_t size);
extern void type_free(Type type);
extern size_t type_len(Type type);
extern bool type_is_proper_subset(const Type a, const Type b,
        ComponentId *diff_component);
extern void type_inspect(const Type type);

// -- Archetype -----------------------------------------------------------------
// Node in an archetype graph. Storage for component data; each column
// corresponding to an entity.
typedef struct Archetype Archetype;

typedef struct ArchetypeEdge ArchetypeEdge;
struct ArchetypeEdge {
    Archetype *add;
    Archetype *remove;
};

struct Archetype {
    Type type;
    size_t current_index;

    // Rows of components.
    Vec(Vec(void)) storage;

    HashMap(ComponentId, ArchetypeEdge) edge_map;
    // Component to row lookup table.
    HashMap(ComponentId, size_t) component_lookup;
    // Column to entity lookup table.
    HashMap(size_t, Entity) entity_lookup;
};

typedef struct ArchetypeColumn ArchetypeColumn;
struct ArchetypeColumn {
    Archetype *archetype;
    size_t index;
};

extern Archetype *archetype_new(const ECS *ecs, Type type);
extern void archetype_free(Archetype *archetype);
// This should only be called on the root archetype that doesn't have any
// component storage.
extern ArchetypeColumn archetype_add_entity(Archetype *archetype, Entity entity);
extern void archetype_move_entity_right(ECS *ecs, Archetype *left, const void *component_data, ComponentId component_id, size_t left_column);
extern void archetype_move_entity_left(ECS *ecs, Archetype *right, ComponentId component_id, size_t right_column);

// -- ECS ----------------------------------------------------------------------
// The central structure connecting every other internal part.
typedef struct Component Component;
struct Component {
    size_t size;
};

typedef HashMap(Str, ComponentId) ComponentMap;

struct ECS {
    ComponentMap component_map;
    Vec(Component) components;

    Archetype * root_archetype;
    HashMap(Type, Archetype *) archetype_map;

    HashMap(Entity, ArchetypeColumn) entity_map;
    HashMap(uint32_t, uint32_t) entity_generation;
    Vec(uint32_t) entity_free_list;
    uint32_t entity_current_id;
};
