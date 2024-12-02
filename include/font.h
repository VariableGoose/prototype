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
    u32 advance;
    Ivec2 size;
    Ivec2 offset;

    u32 codepoint;
    u32 glyph_index;

    // UV's are measured in pixel offsets from the top left corner of the
    // texture atlas.
    // [0] = Top left
    // [1] = Bottom right
    Ivec2 uv[2];
};

typedef HashMap(u32, u32) GlyphMap;

typedef struct RigidFont RigidFont;
struct RigidFont {
};

typedef struct Font Font;
struct Font {
    // Size in pixels
    u32 size;

    u32 descent;
    u32 ascent;
    u32 line_gap;

    Glyph *glyphs;

    // Codepoint to glyph map
    // Key:   u32 (codepoint)
    // Value: u32 (glyph index)
    GlyphMap glyph_map;

    // Monocrhome bitmap written from left to right, top to bottom.
    // Origin in top left.
    struct {
        Ivec2 size;
        u8 *buffer;
    } atlas;
};

extern Font font_init(Str font_path, u32 pixel_height, bool sdf);
extern Font font_init_memory(Str font_data, u32 pixel_height, bool sdf);
extern Glyph font_get_glyph(Font font, u32 codepoint);
extern void font_free(Font font);
