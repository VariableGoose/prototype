#include "gfx.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <glad/gl.h>

#include "linear_algebra.h"
#include "str.h"

// -- Batch Renderer -----------------------------------------------------------
// Batching quads together to reduce draw calls at the expense of using a lot
// more memory and VRAM.
typedef struct BrVertex BrVertex;
struct BrVertex {
    Vec2 pos;
    Vec2 uv;
    Color color;
};

typedef struct BatchRenderer BatchRenderer;
struct BatchRenderer {
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
};

static BatchRenderer br_new(uint32_t max_batch_size) {
    // Multiply by 4 because each quad has 4 vertices.
    uint32_t vertex_count = max_batch_size*4;

    BatchRenderer br = {
        .max_batch_size = max_batch_size,
        .verts = malloc(vertex_count*sizeof(BrVertex)),
    };

    // Create shader.
    int success;
    char info_log[512];

    Str vertex_source = read_file("assets/shaders/br.vert.glsl");
    Str fragment_source = read_file("assets/shaders/br.frag.glsl");

    uint32_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, (const char **) &vertex_source.data, (int *) &vertex_source.len);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        printf("ERROR: Vertex shader compilation failed.\n%s", info_log);
    }

    uint32_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, (const char **) &fragment_source.data, (int *) &fragment_source.len);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        printf("ERROR: Fragment shader compilation failed.\n%s", info_log);
    }

    br.shader = glCreateProgram();
    glAttachShader(br.shader, vertex_shader);
    glAttachShader(br.shader, fragment_shader);
    glLinkProgram(br.shader);
    glGetProgramiv(br.shader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(br.shader, 512, NULL, info_log);
        printf("ERROR: Shader linking failed.\n%s", info_log);
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // This needs to be created and bound before the vertex buffer since the
    // vertex layout gets bound to the vertex array.
    glGenVertexArrays(1, &br.vao);
    glBindVertexArray(br.vao);

    glGenBuffers(1, &br.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, br.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count*sizeof(BrVertex), NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(BrVertex), (const void *) offsetof(BrVertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(BrVertex), (const void *) offsetof(BrVertex, uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, false, sizeof(BrVertex), (const void *) offsetof(BrVertex, color));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    glGenBuffers(1, &br.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, br.ibo);

    // 6 indices per quad for 2 triangles.
    uint32_t index_count = max_batch_size*6;
    uint32_t *indices = malloc(index_count*sizeof(uint32_t));
    uint32_t vert_i = 0;
    for (uint32_t i = 0; i < index_count; i += 6) {
        indices[i+0] = 0 + vert_i;
        indices[i+1] = 1 + vert_i;
        indices[i+2] = 2 + vert_i;

        indices[i+3] = 2 + vert_i;
        indices[i+4] = 3 + vert_i;
        indices[i+5] = 1 + vert_i;

        vert_i += 4;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count*sizeof(uint32_t), indices, GL_STATIC_DRAW);
    // It's fine to free the indices here since glBufferData copies the data to
    // VRAM so it lives there instead of in program memory.
    free(indices);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return br;
}

static void br_free(BatchRenderer br) {
    glDeleteVertexArrays(1, &br.vao);
    glDeleteBuffers(1, &br.vbo);
    glDeleteBuffers(1, &br.ibo);
}

static void br_begin(BatchRenderer *br) {
    br->curr_quad = 0;
}

static void br_end(BatchRenderer *br) {
    glBindBuffer(GL_ARRAY_BUFFER, br->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, br->curr_quad*4*sizeof(BrVertex), br->verts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void br_submit(BatchRenderer *br) {
    glUseProgram(br->shader);

    glBindVertexArray(br->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, br->ibo);

    glDrawElements(GL_TRIANGLES, br->curr_quad*6, GL_UNSIGNED_INT, NULL);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void br_draw_quad(BatchRenderer *br, Quad quad, Color color) {
    const Vec2 uvs[4] = {
        vec2(0.0f, 0.0f),
        vec2(1.0f, 0.0f),
        vec2(0.0f, 1.0f),
        vec2(1.0f, 1.0f),
    };

    const Vec2 pos[4] = {
        vec2(-0.5f, -0.5f),
        vec2( 0.5f, -0.5f),
        vec2(-0.5f,  0.5f),
        vec2( 0.5f,  0.5f),
    };

    for (uint8_t i = 0; i < 4; i++) {
        BrVertex *vert = &br->verts[br->curr_quad*4+i];

        vert->pos = pos[i];
        vert->pos.x *= quad.w;
        vert->pos.y *= quad.h;
        vert->pos.x += quad.x;
        vert->pos.y += quad.y;

        vert->uv = uvs[i];
        vert->color = color;

    }
    br->curr_quad++;
}

// -- Renderer -----------------------------------------------------------------
// User facing renderer api.
struct Renderer {
    BatchRenderer br;
};

Renderer *renderer_new(uint32_t max_batch_size) {
    Renderer *rend = malloc(sizeof(Renderer));
    *rend = (Renderer) {
        .br = br_new(max_batch_size),
    };
    return rend;
}

void renderer_free(Renderer *renderer) {
    br_free(renderer->br);
    free(renderer);
}

void renderer_begin(Renderer *renderer) {
    br_begin(&renderer->br);
}

void renderer_end(Renderer *renderer) {
    br_end(&renderer->br);
}

void renderer_submit(Renderer *renderer) {
    br_submit(&renderer->br);
}

void renderer_draw_quad(Renderer *renderer, Quad quad, Color color) {
    br_draw_quad(&renderer->br, quad, color);
}
