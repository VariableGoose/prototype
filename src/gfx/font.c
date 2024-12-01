#include "font.h"
#include "ds.h"
#include "str.h"

#include <stdlib.h>
#include <stdint.h>
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

    uint32_t ascent;
    uint32_t descent;
    uint32_t line_gap;
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
        .ascent = face_metrics.ascender >> 6,
        .descent = face_metrics.descender >> 6,
        .line_gap = face_metrics.height >> 6,
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

    FT_Done_Face(face);
    FT_Done_FreeType(lib);

    return font_loose;
}

static Font bake_looes_font(FontLoose loose) {
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

        glyphs[i].uv[0] = pos;
        glyphs[i].uv[1] = ivec2_add(pos, curr.bitmap.size);

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

    // Construct atlas bitmap.
    uint8_t *atlas_data = malloc(atlas_size.x*atlas_size.y);
    for (size_t i = 0; i < vec_len(packed_list); i++) {
        PackedGlyph packed = packed_list[i];
        for (int32_t y = 0; y < packed.size.y; y++) {
            for (int32_t x = 0; x < packed.size.x; x++) {
                uint32_t index = packed.pos.x + x + (packed.pos.y + y) * atlas_size.x;
                atlas_data[index] = packed.bitmap[x + y*packed.size.x];
            }
        }
    }

    vec_free(packed_list);

    return (Font) {
        .size = loose.pixel_size,
        .ascent = loose.ascent,
        .descent = loose.descent,
        .line_gap = loose.line_gap,
        .glyphs = glyphs,
        .glyph_map = map,
        .atlas = {
            .buffer = atlas_data,
            .size = atlas_size,
        },
    };
}

Font font_init(Str font_path, uint32_t pixel_height, bool sdf) {
    char *cstr_font_path = malloc(font_path.len + 1);
    memcpy(cstr_font_path, font_path.data, font_path.len);
    cstr_font_path[font_path.len] = 0;
    // TODO: Replace this with some type of scratch/temporary allocator.
    Str ttf_data = read_file(cstr_font_path, ALLOCATOR_LIBC);
    free(cstr_font_path);

    Font font = font_init_memory(ttf_data, pixel_height, sdf);

    free((char *) ttf_data.data);

    return font;
}

Font font_init_memory(Str font_data, uint32_t pixel_height, bool sdf) {
    FontLoose loose_font = construct_loose_font(font_data, pixel_height, sdf);
    Font baked = bake_looes_font(loose_font);
    for (size_t i = 0; i < vec_len(loose_font.glyphs); i++) {
        free(loose_font.glyphs[i].bitmap.buffer);
    }
    vec_free(loose_font.glyphs);
    return baked;
}

Glyph font_get_glyph(Font font, uint32_t codepoint) {
    uint32_t index = hash_map_get(font.glyph_map, codepoint);
    return font.glyphs[index];
}

void font_free(Font font) {
    free(font.glyphs);
    hash_map_free(font.glyph_map);
}
