#pragma once

#include "core.h"
#include "linear_algebra.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef void (*GlFunc)(void);
typedef GlFunc (*GlLoadFunc)(const char *func_name);

extern void gfx_init(GlLoadFunc gl_load_func);

// -- Color --------------------------------------------------------------------
typedef struct Color Color;
struct Color {
    float r, g, b, a;
};

static inline Color color_rgba_f(float r, float g, float b, float a) {
    return (Color) {r, g, b, a};
}

static inline Color color_rgba_i(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (Color) {r/255.0f, g/255.0f, b/255.0f, a/255.0f};
}

static inline Color color_rgba_hex(uint32_t hex) {
    return (Color) {
        .r = (float) (hex >> 8 * 3 & 0xff) / 0xff,
        .g = (float) (hex >> 8 * 2 & 0xff) / 0xff,
        .b = (float) (hex >> 8 * 1 & 0xff) / 0xff,
        .a = (float) (hex >> 8 * 0 & 0xff) / 0xff,
    };
}

static inline Color color_rgb_f(float r, float g, float b) {
    return (Color) {r, g, b, 1.0f};
}

static inline Color color_rgb_i(uint8_t r, uint8_t g, uint8_t b) {
    return (Color) {r/255.0f, g/255.0f, b/255.0f, 1.0f};
}

static inline Color color_rgb_hex(uint32_t hex) {
    return (Color) {
        .r = (float) (hex >> 8 * 2 & 0xff) / 0xff,
        .g = (float) (hex >> 8 * 1 & 0xff) / 0xff,
        .b = (float) (hex >> 8 * 0 & 0xff) / 0xff,
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

// -- Renderer -----------------------------------------------------------------
// Context containing everything neccessary to perform rendering.
typedef struct Renderer Renderer;

extern Renderer *renderer_new(uint32_t max_batch_size, Allocator allocator);
// This function still needs to be called even if renderer was created with a
// lifetime based allocator becaues it also cleans up OpenGL resources, not
// only memory.
extern void renderer_free(Renderer *renderer);

extern void renderer_update(Renderer *renderer, uint32_t screen_width, uint32_t screen_height);
extern void renderer_begin(Renderer *renderer);
extern void renderer_end(Renderer *renderer);
extern void renderer_submit(Renderer *renderer);

typedef struct Quad Quad;
struct Quad {
    int x, y;
    int w, h;
};

typedef uint32_t Texture;

#define TEXTURE_NULL 0

typedef struct TextureAtlas TextureAtlas;
struct TextureAtlas {
    Texture atlas;
    // Top left
    float u0, v0;
    // Bottom left
    float u1, v1;
};

// Draws a quad onto the screen using screen coordinates.
// Origin in the top left corner.
extern void renderer_draw_quad(Renderer *renderer, Quad quad, Texture texture, Color color);
extern void renderer_draw_quad_atlas(Renderer *renderer, Quad quad, TextureAtlas atlas, Color color);

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
    uint32_t width;
    uint32_t height;
    TextureFormat format;
    TextureDataType data_type;
    // Pixels data with the origin beginning at the top left. The amount of
    // pixels is equal to width*height*format.
    const void *pixels;
    TextureSampler sampler;
};

extern Texture texture_new(Renderer *renderer, TextureDesc desc);
extern Ivec2 texture_get_size(const Renderer *renderer, Texture texture);
