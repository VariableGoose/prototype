#pragma once

//
// Standalone module wrapping some backend. Right now, it's Freetype but I
// intend to implement a stb_truetype backend as well.
//

// TODO: Support allocator interface.
// TODO: Implement kerning support.

#include <stdint.h>
#include <stdbool.h>

#include <linear_algebra.h>
#include <ds.h>
#include <str.h>

typedef struct Glyph Glyph;
struct Glyph {
    uint32_t advance;
    Ivec2 size;
    Ivec2 offset;

    uint32_t codepoint;
    uint32_t glyph_index;

    // UV's are measured in pixel offsets from the top left corner of the
    // texture atlas.
    // [0] = Top left
    // [1] = Bottom right
    Ivec2 uv[2];
};

typedef HashMap(uint32_t, uint32_t) GlyphMap;

typedef struct Font Font;
struct Font {
    // Size in pixels
    uint32_t size;

    uint32_t descent;
    uint32_t ascent;
    uint32_t line_gap;

    Glyph *glyphs;

    // Codepoint to glyph map
    // Key:   uint32_t (codepoint)
    // Value: uint32_t (glyph index)
    GlyphMap glyph_map;

    // Monocrhome bitmap written from left to right, top to bottom.
    // Origin in top left.
    struct {
        Ivec2 size;
        uint8_t *buffer;
    } atlas;
};

extern Font font_init(Str font_path, uint32_t pixel_height, bool sdf);
extern Font font_init_memory(Str font_data, uint32_t pixel_height, bool sdf);
extern Glyph font_get_glyph(Font font, uint32_t codepoint);
extern void font_free(Font font);
