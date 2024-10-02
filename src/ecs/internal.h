#pragma once

#include "ecs.h"
#include "ds.h"

typedef size_t ComponentId;

typedef struct Archetype Archetype;

typedef struct ArchetypeEdge ArchetypeEdge;
struct ArchetypeEdge {
    Archetype *add;
    Archetype *remove;
};

typedef Vec(ComponentId) Type;

struct Archetype {
    Type type;
    // TODO: Store each component in a separate vector since operations are
    // going to operate on individual components and not whole archetypes.
    Vec(void) storage;
    HashMap(ComponentId, ArchetypeEdge) edge_map;
    HashMap(ComponentId, size_t) component_index_map;
};

typedef struct ArchetypeColumn ArchetypeColumn;
struct ArchetypeColumn {
    Archetype *archetype;
    size_t index;
};

typedef struct Component Component;
struct Component {
    size_t size;
};

typedef HashMap(Str, ComponentId) ComponentMap;

struct ECS {
    ComponentMap component_map;
    Vec(Component) components;

    Vec(Archetype) archetypes;
    HashMap(Type, Archetype *) archetype_map;

    HashMap(uint32_t, ArchetypeColumn) entity_map;
    Vec(uint32_t) entity_generation;
    Vec(uint32_t) entity_free_list;
    uint32_t entity_curr_index;
};

extern Archetype archetype_new(const ECS *ecs, Type type);
extern void archetype_free(Archetype archetype);
extern ArchetypeColumn archetype_column_new(Archetype *archetype);
extern ArchetypeColumn archetype_move(Archetype *current, Archetype *new, size_t index);

extern void type_add(Type type, ComponentId component);
extern void type_remove(Type type, ComponentId component);
extern Type type_clone(const Type type);
extern size_t type_hash(const void *data, size_t size);
extern int type_cmp(const void *a, const void *b, size_t size);
extern void type_free(Type type);
extern size_t type_len(Type type);
extern bool type_is_proper_subset(const Type a, const Type b);
