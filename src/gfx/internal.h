#pragma once

#include "core.h"
#include "gfx.h"
#include "linear_algebra.h"
#include <ds.h>

typedef struct TextureInternal TextureInternal;
struct TextureInternal {
    // OpenGL texture ID
    uint32_t id;

    uint32_t width;
    uint32_t height;
};

typedef struct BrVertex BrVertex;
struct BrVertex {
    Vec2 pos;
    Vec2 uv;
    Color color;
    uint32_t texture_index;
};

typedef struct BatchRenderer BatchRenderer;
struct BatchRenderer {
    Allocator allocator;

    // Vertex array
    uint32_t vao;
    // Vertex buffer
    uint32_t vbo;
    // Index buffer
    uint32_t ibo;
    uint32_t shader;

    uint32_t max_batch_size;
    BrVertex *verts;
    uint32_t curr_quad;

    Texture textures[32];
    uint32_t curr_texture;

    struct {
        uint32_t width;
        uint32_t height;
    } screen;
};

struct Renderer {
    Allocator allocator;
    BatchRenderer br;
    Vec(TextureInternal) textures;
};
