#include "core.h"
#include "ds.h"
#include "gfx.h"
#include "internal.h"

#include <stdlib.h>
#include <stdio.h>

#include <glad/gl.h>
#include <unibreakdef.h>

#include "str.h"

// -- Batch Renderer -----------------------------------------------------------
// Batching quads together to reduce draw calls at the expense of using a lot
// more memory and VRAM.
typedef struct TextureAtlasInternal TextureAtlasInternal;
struct TextureAtlasInternal {
    uint32_t id;
    float u0, v0;
    float u1, v1;
};

static BatchRenderer br_new(uint32_t max_batch_size, Allocator allocator) {
    // Multiply by 4 because each quad has 4 vertices.
    uint32_t vertex_count = max_batch_size*4;

    BatchRenderer br = {
        .allocator = allocator,
        .max_batch_size = max_batch_size,
        .verts = allocator.alloc(vertex_count*sizeof(BrVertex), allocator.ctx),
        // .verts = malloc(vertex_count*sizeof(BrVertex)),
    };

    // Create shader.
    int success;
    char info_log[512];

    // TODO: Replace 'ALLOCATOR_LIBC' with a scratch arena or another form of
    // temporary allocator.
    Str vertex_source = read_file("assets/shaders/br.vert.glsl", ALLOCATOR_LIBC);
    Str fragment_source = read_file("assets/shaders/br.frag.glsl", ALLOCATOR_LIBC);

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

    free((char *) vertex_source.data);
    free((char *) fragment_source.data);

    glUseProgram(br.shader);
    uint32_t location = glGetUniformLocation(br.shader, "textures");
    int32_t samplers[32] = {0};
    for (uint32_t i = 0; i < arrlen(samplers); i++) {
        samplers[i] = i;
    }
    glUniform1iv(location, arrlen(samplers), samplers);
    glUseProgram(0);

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
    glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, sizeof(BrVertex), (const void *) offsetof(BrVertex, texture_index));
    glEnableVertexAttribArray(3);

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
    br.allocator.free(br.verts, br.max_batch_size*4*sizeof(BrVertex), br.allocator.ctx);
}

static void br_begin(BatchRenderer *br, Camera camera) {
    br->camera = camera;
    br->curr_quad = 0;
    br->curr_texture = 0;
}

static void br_end(BatchRenderer *br) {
    glBindBuffer(GL_ARRAY_BUFFER, br->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, br->curr_quad*4*sizeof(BrVertex), br->verts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void br_submit(BatchRenderer *br) {
    glUseProgram(br->shader);
    for (uint32_t i = 0; i < br->curr_texture; i++) {
        glActiveTexture(GL_TEXTURE0+i);
        glBindTexture(GL_TEXTURE_2D, br->textures[i]);
    }

    glUniform2iv(glGetUniformLocation(br->shader, "cam.screen_size"), 1, &br->camera.screen_size.x);
    glUniform1f(glGetUniformLocation(br->shader, "cam.zoom"), br->camera.zoom);
    glUniform2fv(glGetUniformLocation(br->shader, "cam.pos"), 1, &br->camera.position.x);
    glUniform2fv(glGetUniformLocation(br->shader, "cam.dir"), 1, &br->camera.direction.x);

    f32 aspect = (f32) br->camera.screen_size.x / (f32) br->camera.screen_size.y;
    f32 zoom = br->camera.zoom / 2.0f;
    Mat4 projection = mat4_ortho_projection(-aspect*zoom, aspect*zoom, zoom, -zoom, 1.0f, -1.0f);
    glUniformMatrix4fv(glGetUniformLocation(br->shader, "projection"), 1, false, &projection.a.x);

    glBindVertexArray(br->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, br->ibo);

    glDrawElements(GL_TRIANGLES, br->curr_quad*6, GL_UNSIGNED_INT, NULL);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

static void br_draw_aabb(BatchRenderer *br, AABB aabb, Vec2 origin, TextureAtlasInternal atlas, Color color) {
    if (br->curr_quad == br->max_batch_size || br->curr_texture == 32) {
        br_end(br);
        br_submit(br);
        br_begin(br, br->camera);
    }

    uint32_t texture_index = 0;
    b8 new_texture = true;
    for (uint32_t i = 0; i < br->curr_texture; i++) {
        if (br->textures[i] == atlas.id) {
            texture_index = i;
            new_texture = false;
            break;
        }
    }

    if (new_texture) {
        br->textures[br->curr_texture] = atlas.id;
        texture_index = br->curr_texture;
        br->curr_texture++;
    }

    const Vec2 uvs[4] = {
        vec2(atlas.u0, atlas.v1),
        vec2(atlas.u1, atlas.v1),
        vec2(atlas.u0, atlas.v0),
        vec2(atlas.u1, atlas.v0),
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
        vert->pos = vec2_sub(vert->pos, vec2_divs(vec2_mul(origin, br->camera.direction), 2.0f));
        vert->pos = vec2_mul(vert->pos, aabb.size);
        vert->pos = vec2_add(vert->pos, vec2_mul(aabb.position, br->camera.direction));
        vert->pos = vec2_sub(vert->pos, vec2_mul(br->camera.position, br->camera.direction));

        vert->uv = uvs[i];
        vert->color = color;
        vert->texture_index = texture_index;
    }
    br->curr_quad++;
}

// -- Renderer -----------------------------------------------------------------
// User facing renderer api.
Renderer *renderer_new(uint32_t max_batch_size, Allocator allocator) {
    Renderer *rend = allocator.alloc(sizeof(Renderer), allocator.ctx);
    *rend = (Renderer) {
        .allocator = allocator,
        .br = br_new(max_batch_size, allocator),
    };
    // Texture index 0 is the NULL white texture.
    texture_new(rend, (TextureDesc) {
            .width = 1,
            .height = 1,
            .format = TEXTURE_FORMAT_RGBA,
            .data_type = TEXTURE_DATA_TYPE_UBYTE,
            .sampler = TEXTURE_SAMPLER_NEAREST,
            .pixels = (u8[]) {
                255, 255, 255, 255,
            },
        });
    return rend;
}

void renderer_free(Renderer *renderer) {
    for (size_t i = 0; i < vec_len(renderer->textures); i++) {
        glDeleteTextures(1, &renderer->textures[i].id);
    }
    vec_free(renderer->textures);

    br_free(renderer->br);
    renderer->allocator.free(renderer, sizeof(*renderer), renderer->allocator.ctx);
}

void renderer_begin(Renderer *renderer, Camera camera) {
    br_begin(&renderer->br, camera);
}

void renderer_end(Renderer *renderer) {
    br_end(&renderer->br);
}

void renderer_submit(Renderer *renderer) {
    br_submit(&renderer->br);
}

void renderer_draw_aabb(Renderer *renderer, AABB aabb, Vec2 origin, Texture texture, Color color) {
    br_draw_aabb(&renderer->br,
            aabb,
            origin,
            (TextureAtlasInternal) {
                renderer->textures[texture].id,
                0, 0,
                1, 1
            },
            color);
}

void renderer_draw_aabb_atlas(Renderer *renderer, AABB aabb, Vec2 origin, TextureAtlas atlas, Color color) {
    br_draw_aabb(&renderer->br,
            aabb,
            origin,
            (TextureAtlasInternal) {
                .id = renderer->textures[atlas.atlas].id,
                .u0 = atlas.u0,
                .v0 = atlas.v0,
                .u1 = atlas.u1,
                .v1 = atlas.v1,
            },
            color);
}

Vec2 renderer_draw_string(Renderer *renderer, Str string, Font *font, u32 size, Vec2 position, Color color) {
    FontMetrics metrics = font_get_metrics(font, size);
    Vec2 str_size = vec2(0.0f, metrics.ascent-metrics.descent);
    for (u64 i = 0; i < string.len; i++) {
        Glyph glyph = font_get_glyph(font, string.data[i], size);
        renderer_draw_aabb_atlas(renderer, (AABB) {
                .position = {
                    .x = position.x + glyph.offset.x,
                    .y = position.y - glyph.offset.y + metrics.ascent,
                },
                .size = vec2(vec2_arg(glyph.size)),
            }, vec2(-1.0f, -1.0f), (TextureAtlas) {
                .atlas = font_get_atlas(font, size),
                .u0 = glyph.uv[0].x,
                .v0 = glyph.uv[0].y,
                .u1 = glyph.uv[1].x,
                .v1 = glyph.uv[1].y,
            }, color);
        position.x += glyph.advance;
        str_size.x += glyph.advance;
    }
    return str_size;
}
