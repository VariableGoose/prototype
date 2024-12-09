#pragma once

#include "core.h"
#include "gfx.h"
#include "linear_algebra.h"
#include <ds.h>

typedef struct TextureInternal TextureInternal;
struct TextureInternal {
    // OpenGL texture ID
    u32 id;

    Ivec2 size;
};

typedef struct BrVertex BrVertex;
struct BrVertex {
    Vec2 pos;
    Vec2 uv;
    Color color;
    u32 texture_index;
};

typedef struct BatchRenderer BatchRenderer;
struct BatchRenderer {
    Allocator allocator;

    // Vertex array
    u32 vao;
    // Vertex buffer
    u32 vbo;
    // Index buffer
    u32 ibo;
    u32 shader;

    u32 max_batch_size;
    BrVertex *verts;
    u32 curr_quad;

    Texture textures[32];
    u32 curr_texture;

    Camera camera;
};

struct Renderer {
    Allocator allocator;
    BatchRenderer br;
    Vec(TextureInternal) textures;
};
