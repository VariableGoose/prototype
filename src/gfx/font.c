#include "font.h"
#include "core.h"
#include "ds.h"
#include "gfx.h"
#include "str.h"

#include <stdlib.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct GlyphLoose GlyphLoose;
struct GlyphLoose {
    GlyphLoose *next;

    uint32_t codepoint;
    uint32_t glyph_index;
    uint32_t advance;

    Ivec2 size;
    Ivec2 offset;

    struct {
        Ivec2 size;
        uint8_t *buffer;
    } bitmap;
};

typedef struct FontLoose FontLoose;
struct FontLoose {
    Vec(GlyphLoose) glyphs;

    uint32_t pixel_size;

    FontMetrics metrics;
};

static void process_glyph(FontLoose *font_loose, FT_Face face, uint32_t glyph_index, uint32_t codepoint, bool sdf) {
    FT_Error err = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    if (err) {
        printf("Some glyph error occured.");
    }

    if (sdf) {
        err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);
    } else {
        err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    }
    if (err) {
        printf("Some render error occured");
    }

    FT_Glyph_Metrics metrics = face->glyph->metrics;
    FT_Bitmap bitmap = face->glyph->bitmap;

    uint8_t *bitmap_copy = malloc(bitmap.width*bitmap.rows);
    memcpy(bitmap_copy, bitmap.buffer, bitmap.width*bitmap.rows);

    GlyphLoose loose = {
        .codepoint = codepoint,
        .glyph_index = glyph_index,

        .size = ivec2(metrics.width >> 6, metrics.height >> 6),
        .offset = ivec2(metrics.horiBearingX >> 6, metrics.horiBearingY >> 6),

        .advance = metrics.horiAdvance >> 6,
        .bitmap = {
            .size = ivec2(bitmap.width, bitmap.rows),
            .buffer = bitmap_copy,
        },
    };

    vec_push(font_loose->glyphs, loose);
}

// static void extract_kerning(FontLoose *loose, FT_Face face) {
//     for (u64 left = 0; left < vec_len(loose->glyphs); left++) {
//         for (u64 right = 0; right < vec_len(loose->glyphs); right++) {
//             FT_Vector kerning_vector;
//             GlyphLoose left_glyph = loose->glyphs[left];
//             GlyphLoose right_glyph = loose->glyphs[right];
//             FT_Error err = FT_Get_Kerning(face, left_glyph.glyph_index, right_glyph.glyph_index, FT_KERNING_DEFAULT, &kerning_vector);
//             if (err != 0) {
//                 printf("Kerning ain't working cheif\n");
//             }
//             if (left_glyph.codepoint == 'A' && right_glyph.codepoint == 'V') {
//                 printf("%c%c : (%ld, %ld)\n", left_glyph.codepoint, right_glyph.codepoint, kerning_vector.x >> 6, kerning_vector.y >> 6);
//             }
//             if (kerning_vector.x != 0 || kerning_vector.y != 0) {
//                 printf("%c%c : (%ld, %ld)\n", left_glyph.codepoint, right_glyph.codepoint, kerning_vector.x >> 6, kerning_vector.y >> 6);
//             }
//         }
//     }
// }

static FontLoose construct_loose_font(Str font_data, uint32_t pixel_size, bool sdf) {
    FT_Library lib;
    FT_Error err = FT_Init_FreeType(&lib);
    if (err) {
        printf("Some error occured");
    }

    FT_Face face;
    err = FT_New_Memory_Face(lib, (const FT_Byte *) font_data.data, font_data.len, 0, &face);
    if (err) {
        printf("Some error occured");
    }

    err = FT_Set_Pixel_Sizes(face, 0, pixel_size);
    if (err) {
        printf("Non-variable size!");
    }

    FT_Size_Metrics face_metrics = face->size->metrics;

    FontLoose font_loose = {
        .metrics = {
            .ascent = face_metrics.ascender >> 6,
            .descent = face_metrics.descender >> 6,
            .line_gap = face_metrics.height >> 6,
        },
        .pixel_size = pixel_size,
    };

    // The 'missing glyph' glyph is always located at glyph index 0 within the font.
    // So load that first.
    uint32_t glyph_index = 0;
    uint32_t codepoint = 0;
    process_glyph(&font_loose, face, glyph_index, codepoint, sdf);

    codepoint = FT_Get_First_Char(face, &glyph_index);
    while (glyph_index != 0) {
        process_glyph(&font_loose, face, glyph_index, codepoint, sdf);
        codepoint = FT_Get_Next_Char(face, codepoint, &glyph_index);
    }

    // if (FT_HAS_KERNING(face)) {
    //     extract_kerning(&font_loose, face);
    // } else {
    //     printf("Typeface doesn't support kerning.\n");
    // }

    FT_Done_Face(face);
    FT_Done_FreeType(lib);

    return font_loose;
}

static RigidFont bake_looes_font(FontLoose loose, Renderer *renderer) {
    GlyphMap map = NULL;

    Glyph *glyphs = malloc(sizeof(Glyph) * vec_len(loose.glyphs));

    typedef struct PackedGlyph PackedGlyph;
    struct PackedGlyph {
        Ivec2 pos;
        Ivec2 size;
        uint8_t *bitmap;
    };

    Vec(PackedGlyph) packed_list = NULL;

    Ivec2 atlas_size = ivec2s(1);
    Ivec2 origin = ivec2s(0);
    Ivec2 pos = ivec2s(0);
    int32_t row_height = 0;

    for (size_t i = 0; i < vec_len(loose.glyphs); i++) {
        GlyphLoose curr = loose.glyphs[i];
        glyphs[i] = (Glyph) {
            .size = curr.size,
            // .size = curr.bitmap.size,
            .offset = curr.offset,
            .codepoint = curr.codepoint,
            .glyph_index = curr.glyph_index,
            .advance = curr.advance,
        };

        // Place in atlas.
        while (curr.bitmap.size.x >= atlas_size.x || curr.bitmap.size.y >= atlas_size.y) {
            if (atlas_size.x != atlas_size.y) {
                atlas_size.x *= 2;
            } else {
                atlas_size.y *= 2;
            }
        }

        if (pos.x + curr.bitmap.size.x >= atlas_size.x) {
            pos.x = origin.x;
            pos.y += row_height;
            row_height = 0;
        }

        if (pos.y + curr.bitmap.size.y >= atlas_size.y) {
            if (atlas_size.x == atlas_size.y) {
                origin = ivec2(atlas_size.x, 0);
                atlas_size.x *= 2;
            } else {
                origin = ivec2(0, atlas_size.y);
                atlas_size.y *= 2;
            }
            pos = origin;
        }

        if (row_height < curr.bitmap.size.y) {
            row_height = curr.bitmap.size.y;
        }

        glyphs[i].uv[0] = vec2(vec2_arg(pos));
        glyphs[i].uv[1] = vec2(vec2_arg(ivec2_add(pos, curr.bitmap.size)));

        // Store all packed glyphs in a stack for rendering into atlas.
        PackedGlyph packed = {
            .pos = pos,
            .size = curr.bitmap.size,
            .bitmap = curr.bitmap.buffer,
        };
        vec_push(packed_list, packed);

        pos.x += curr.bitmap.size.x;

        hash_map_insert(map, curr.codepoint, i);
    }

    for (u64 i = 0; i < vec_len(loose.glyphs); i++) {
        for (u8 j = 0; j < 2; j++) {
            glyphs[i].uv[j] = vec2_div(glyphs[i].uv[j], vec2(vec2_arg(atlas_size)));
        }
    }

    // Construct atlas bitmap.
    uint8_t *atlas_data = malloc(atlas_size.x*atlas_size.y*4);
    for (size_t i = 0; i < vec_len(packed_list); i++) {
        PackedGlyph packed = packed_list[i];
        for (int32_t y = 0; y < packed.size.y; y++) {
            for (int32_t x = 0; x < packed.size.x; x++) {
                uint32_t index = packed.pos.x + x + (packed.pos.y + y) * atlas_size.x;
                atlas_data[index*4+0] = 255;
                atlas_data[index*4+1] = 255;
                atlas_data[index*4+2] = 255;
                atlas_data[index*4+3] = packed.bitmap[x + y*packed.size.x];
            }
        }
    }

    // char name[64] = {0};
    // snprintf(name, 64, "atlas%d.png", rand());
    // stbi_write_png(name, atlas_size.x, atlas_size.y, 4, atlas_data, atlas_size.x*4);

    Texture atlas = texture_new(renderer, (TextureDesc) {
            .width = atlas_size.x,
            .height = atlas_size.y,
            .format = TEXTURE_FORMAT_RGBA,
            .sampler = TEXTURE_SAMPLER_LINEAR,
            .data_type = TEXTURE_DATA_TYPE_UBYTE,
            .pixels = atlas_data,
        });

    free(atlas_data);

    vec_free(packed_list);

    return (RigidFont) {
        .size = loose.pixel_size,
        .metrics = loose.metrics,
        .glyphs = glyphs,
        .glyph_map = map,
        .atlas = atlas,
    };
}

Font font_init(Str font_path, Renderer *renderer, b8 sdf, Allocator allocator) {
    // TODO: Replace this with some type of scratch/temporary allocator.
    char *cstr_font_path = malloc(font_path.len + 1);
    memcpy(cstr_font_path, font_path.data, font_path.len);
    cstr_font_path[font_path.len] = 0;
    Font font = {
        .allocator = allocator,
        .renderer = renderer,
        .ttf_data = read_file(cstr_font_path, allocator),
        .sdf = sdf,
    };
    free(cstr_font_path);

    return font;
}

Font font_init_memory(Str font_data, Renderer *renderer, b8 sdf, Allocator allocator) {
    Font font = {
        .allocator = allocator,
        .renderer = renderer,
        .ttf_data = str_copy(font_data, allocator),
        .sdf = sdf,
    };
    return font;
}

void font_free(Font *font) {
    for (HashMapIter i = hash_map_iter_new(font->size_lookup);
            hash_map_iter_valid(font->size_lookup, i);
            i = hash_map_iter_next(font->size_lookup, i)) {
        RigidFont rigid = font->size_lookup[i].value;
        free(rigid.glyphs);
        hash_map_free(rigid.glyph_map);
    }
    hash_map_free(font->size_lookup);
    free((u8 *) font->ttf_data.data);
}

void font_cache_size(Font *font, u32 size) {
    if (hash_map_getp(font->size_lookup, size) != NULL) {
        return;
    }

    FontLoose loose = construct_loose_font(font->ttf_data, size, font->sdf);
    RigidFont rigid = bake_looes_font(loose, font->renderer);
    for (u32 i = 0; i < vec_len(loose.glyphs); i++) {
        free(loose.glyphs[i].bitmap.buffer);
    }
    vec_free(loose.glyphs);
    hash_map_insert(font->size_lookup, size, rigid);
}

Glyph font_get_glyph(Font *font, u32 codepoint, u32 size) {
    RigidFont *rigid = hash_map_getp(font->size_lookup, size);
    if (rigid == NULL) {
        font_cache_size(font, size);
        rigid = hash_map_getp(font->size_lookup, size);
    }
    u32 index = hash_map_get(rigid->glyph_map, codepoint);
    return rigid->glyphs[index];
}

Texture font_get_atlas(Font *font, u32 size) {
    RigidFont *rigid = hash_map_getp(font->size_lookup, size);
    if (rigid == NULL) {
        font_cache_size(font, size);
        rigid = hash_map_getp(font->size_lookup, size);
    }
    return rigid->atlas;
}

FontMetrics font_get_metrics(Font *font, u32 size) {
    RigidFont *rigid = hash_map_getp(font->size_lookup, size);
    if (rigid == NULL) {
        font_cache_size(font, size);
        rigid = hash_map_getp(font->size_lookup, size);
    }
    return rigid->metrics;
}

Ivec2 font_measure_string(Font *font, Str str, u32 size) {
    RigidFont *rigid = hash_map_getp(font->size_lookup, size);
    if (rigid == NULL) {
        font_cache_size(font, size);
        rigid = hash_map_getp(font->size_lookup, size);
    }

    Ivec2 str_size = ivec2(0, rigid->metrics.ascent - rigid->metrics.descent);
    for (u64 i = 0; i < str.len; i++) {
        Glyph glyph = rigid->glyphs[hash_map_get(rigid->glyph_map, str.data[i])];
        str_size.x += glyph.advance;
    }
    return str_size;
}
