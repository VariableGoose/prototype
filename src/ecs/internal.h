#pragma once

#include "ecs.h"
#include "ds.h"

typedef size_t ComponentId;

typedef struct Archetype Archetype;
struct Archetype {
    Vec(ComponentId) types;
    Vec(void) storage;
};

typedef struct ArchetypeColumn ArchetypeColumn;
struct ArchetypeColumn {
    const Archetype *archetype;
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
    HashMap(uint32_t, ArchetypeColumn) entity_map;

    Vec(uint32_t) entity_generation;
    Vec(uint32_t) entity_free_list;
    uint32_t entity_curr_index;
};

extern Archetype archetype_new(const ECS *ecs, Vec(ComponentId) types);
extern void archetype_free(Archetype archetype);
extern ArchetypeColumn archetype_column_new(Archetype *archetype);
