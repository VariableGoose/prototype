#pragma once

#include "core.h"
#include <math.h>
#include "str.h"

typedef void (*GlFunc)(void);
typedef GlFunc (*GlLoadFunc)(const char *func_name);

extern void gfx_init(GlLoadFunc gl_load_func);

// -- Color --------------------------------------------------------------------
typedef struct Color Color;
struct Color {
    f32 r, g, b, a;
};

static inline Color color_rgba_f(f32 r, f32 g, f32 b, f32 a) {
    return (Color) {r, g, b, a};
}

static inline Color color_rgba_i(u8 r, u8 g, u8 b, u8 a) {
    return (Color) {r/255.0f, g/255.0f, b/255.0f, a/255.0f};
}

static inline Color color_rgba_hex(u32 hex) {
    return (Color) {
        .r = (f32) (hex >> 8 * 3 & 0xff) / 0xff,
        .g = (f32) (hex >> 8 * 2 & 0xff) / 0xff,
        .b = (f32) (hex >> 8 * 1 & 0xff) / 0xff,
        .a = (f32) (hex >> 8 * 0 & 0xff) / 0xff,
    };
}

static inline Color color_rgb_f(f32 r, f32 g, f32 b) {
    return (Color) {r, g, b, 1.0f};
}

static inline Color color_rgb_i(u8 r, u8 g, u8 b) {
    return (Color) {r/255.0f, g/255.0f, b/255.0f, 1.0f};
}

static inline Color color_rgb_hex(u32 hex) {
    return (Color) {
        .r = (f32) (hex >> 8 * 2 & 0xff) / 0xff,
        .g = (f32) (hex >> 8 * 1 & 0xff) / 0xff,
        .b = (f32) (hex >> 8 * 0 & 0xff) / 0xff,
        .a = 1.0f,
    };
}

static inline Color color_hsl(f32 hue, f32 saturation, f32 lightness) {
    // https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB
    Color color = {0};
    f32 chroma = (1 - fabsf(2 * lightness - 1)) * saturation;
    f32 hue_prime = fabsf(fmodf(hue, 360.0f)) / 60.0f;
    f32 x = chroma * (1.0f - fabsf(fmodf(hue_prime, 2.0f) - 1.0f));
    if (hue_prime < 1.0f) { color = (Color) { chroma, x, 0.0f, 1.0f, }; }
    else if (hue_prime < 2.0f) { color = (Color) { x, chroma, 0.0f, 1.0f, }; }
    else if (hue_prime < 3.0f) { color = (Color) { 0.0f, chroma, x, 1.0f, }; }
    else if (hue_prime < 4.0f) { color = (Color) { 0.0f, x, chroma, 1.0f, }; }
    else if (hue_prime < 5.0f) { color = (Color) { x, 0.0f, chroma, 1.0f, }; }
    else if (hue_prime < 6.0f) { color = (Color) { chroma, 0.0f, x, 1.0f, }; }
    f32 m = lightness-chroma / 2.0f;
    color.r += m;
    color.g += m;
    color.b += m;
    return color;
}

static inline Color color_hsv(f32 hue, f32 saturation, f32 value) {
    // https://en.wikipedia.org/wiki/HSL_and_HSV#HSV_to_RGB
    Color color = {0};
    f32 chroma = value * saturation;
    f32 hue_prime = fabsf(fmodf(hue, 360.0f)) / 60.0f;
    f32 x = chroma * (1.0f - fabsf(fmodf(hue_prime, 2.0f) - 1.0f));
    if (hue_prime < 1.0f) { color = (Color) { chroma, x, 0.0f, 1.0f, }; }
    else if (hue_prime < 2.0f) { color = (Color) { x, chroma, 0.0f, 1.0f, }; }
    else if (hue_prime < 3.0f) { color = (Color) { 0.0f, chroma, x, 1.0f, }; }
    else if (hue_prime < 4.0f) { color = (Color) { 0.0f, x, chroma, 1.0f, }; }
    else if (hue_prime < 5.0f) { color = (Color) { x, 0.0f, chroma, 1.0f, }; }
    else if (hue_prime < 6.0f) { color = (Color) { chroma, 0.0f, x, 1.0f, }; }
    f32 m = value - chroma;
    color.r += m;
    color.g += m;
    color.b += m;
    return color;
}

#define color_arg(color) (color).r, (color).g, (color).b, (color).a

#define COLOR_WHITE ((Color) {1.0f, 1.0f, 1.0f, 1.0f})
#define COLOR_BLACK ((Color) {0.0f, 0.0f, 0.0f, 1.0f})
#define COLOR_RED ((Color) {1.0f, 0.0f, 0.0f, 1.0f})
#define COLOR_GREEN ((Color) {0.0f, 1.0f, 0.0f, 1.0f})
#define COLOR_BLUE ((Color) {0.0f, 0.0f, 1.0f, 1.0f})

// -- Camera -------------------------------------------------------------------

typedef struct Camera Camera;
struct Camera {
    Ivec2 screen_size;
    f32 zoom;
    Vec2 position;
    // Which direction is right and up.
    Vec2 direction;
};

extern Vec2 screen_to_world_space(Camera camera, Vec2 screen);
extern Vec2 world_to_screen_space(Camera camera, Vec2 world);

// -- Renderer -----------------------------------------------------------------
// Context containing everything neccessary to perform rendering.
typedef struct Renderer Renderer;

extern Renderer *renderer_new(u32 max_batch_size, Allocator allocator);
// This function still needs to be called even if renderer was created with a
// lifetime based allocator becaues it also cleans up OpenGL resources, not
// only memory.
extern void renderer_free(Renderer *renderer);

extern void renderer_begin(Renderer *renderer, Camera camera);
extern void renderer_end(Renderer *renderer);
extern void renderer_submit(Renderer *renderer);

typedef u32 Texture;
typedef struct Font Font;

typedef struct TextureAtlas TextureAtlas;
struct TextureAtlas {
    Texture atlas;
    // Top left
    f32 u0, v0;
    // Bottom left
    f32 u1, v1;
};

// Draws a quad onto the screen using screen coordinates.
// The origin looks like this when the direction of the camera is (1, 1).
// (-1,  1) - (1,  1)
// |                |
// |                |
// |                |
// |      (0, 0)    |
// |                |
// |                |
// |                |
// (-1, -1) - (1, -1)
extern void renderer_draw_aabb(Renderer *renderer, AABB aabb, Vec2 origin, Texture texture, Color color);
extern void renderer_draw_aabb_atlas(Renderer *renderer, AABB aabb, Vec2 origin, TextureAtlas atlas, Color color);

// Made to be used during a pass with a camera configured to screen space.
// (Camera) {
//     .screen_size = screen_size,
//     .zoom = screen_size.y,
//     .position = vec2(vec2_arg(ivec2_divs(screen_size, 2))),
//     .direction = vec2(1.0f, -1.0f),
// }
extern Vec2 renderer_draw_string(Renderer *renderer, Str string, Font *font, u32 size, Vec2 position, Color color);

// -- Font ---------------------------------------------------------------------
// Freetype2 doesn't seem to parse GPOS kerning. Since almost all modern
// fonts use GPOS as their kerning data it's not worth supporting the feature
// for such a small subset of fonts. At least for now.

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

typedef struct FontMetrics FontMetrics;
struct FontMetrics {
    i32 descent;
    i32 ascent;
    i32 line_gap;
};

extern Font *font_init(Str font_path, Renderer *renderer, b8 sdf, Allocator allocator);
extern Font *font_init_memory(Str font_data, Renderer *renderer, b8 sdf, Allocator allocator);
extern void font_free(Font *font);
extern void font_cache_size(Font *font, u32 size);

extern Glyph font_get_glyph(Font *font, u32 codepoint, u32 size);
extern Texture font_get_atlas(Font *font, u32 size);
extern FontMetrics font_get_metrics(Font *font, u32 size);
// This function ignores vertical space. New-line characters are treated as
// unknown characters.
extern Vec2 font_measure_string(Font *font, Str str, u32 size);

// -- Texture ------------------------------------------------------------------
typedef enum {
    TEXTURE_SAMPLER_LINEAR,
    TEXTURE_SAMPLER_NEAREST,
} TextureSampler;

typedef enum {
    TEXTURE_FORMAT_R = 1,
    TEXTURE_FORMAT_RG = 2,
    TEXTURE_FORMAT_RGB = 3,
    TEXTURE_FORMAT_RGBA = 4,
} TextureFormat;

typedef enum {
    TEXTURE_DATA_TYPE_BYTE,
    TEXTURE_DATA_TYPE_UBYTE,

    TEXTURE_DATA_TYPE_INT,
    TEXTURE_DATA_TYPE_UINT,

    TEXTURE_DATA_TYPE_FLOAT16,
    TEXTURE_DATA_TYPE_FLOAT32,
} TextureDataType;

typedef struct TextureDesc TextureDesc;
struct TextureDesc {
    u32 width;
    u32 height;
    TextureFormat format;
    TextureDataType data_type;
    // Pixels data with the origin beginning at the top left. The amount of
    // pixels is equal to width*height*format.
    const void *pixels;
    TextureSampler sampler;
};

#define TEXTURE_NULL 0

extern Texture texture_new(Renderer *renderer, TextureDesc desc);
extern Ivec2 texture_get_size(const Renderer *renderer, Texture texture);
