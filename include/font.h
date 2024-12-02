#pragma once

//
// Standalone module wrapping some backend. Right now, it's Freetype but I
// intend to implement a stb_truetype backend as well.
//

// TODO: Support allocator interface.
// TODO: Implement kerning support.

#include <stdint.h>
#include <stdbool.h>

#include "linear_algebra.h"
#include "ds.h"
#include "str.h"
#include "gfx.h"

typedef struct Glyph Glyph;
struct Glyph {
    u32 advance;
    Ivec2 size;
    Ivec2 offset;

    u32 codepoint;
    u32 glyph_index;

    // Normalized UV coordinates.
    // [0] = Top left
    // [1] = Bottom right
    Vec2 uv[2];
};

typedef HashMap(u32, u32) GlyphMap;

typedef struct FontMetrics FontMetrics;
struct FontMetrics {
    u32 descent;
    u32 ascent;
    u32 line_gap;
};

typedef struct RigidFont RigidFont;
struct RigidFont {
    u32 size;

    Glyph *glyphs;
    FontMetrics metrics;

    // Codepoint to glyph map
    // Key:   u32 (codepoint)
    // Value: u32 (glyph index)
    GlyphMap glyph_map;

    Texture atlas;
};

typedef struct Font Font;
struct Font {
    Allocator allocator;
    Renderer *renderer;
    Str ttf_data;
    b8 sdf;
    HashMap(u32, RigidFont) size_lookup;
};

extern Font font_init(Str font_path, Renderer *renderer, b8 sdf, Allocator allocator);
extern Font font_init_memory(Str font_data, Renderer *renderer, b8 sdf, Allocator allocator);
extern void font_free(Font *font);
extern void font_cache_size(Font *font, u32 size);

extern Glyph font_get_glyph(Font *font, u32 codepoint, u32 size);
extern Texture font_get_atlas(Font *font, u32 size);
extern FontMetrics font_get_metrics(Font *font, u32 size);
extern Ivec2 font_measure_string(Font *font, Str str, u32 size);
