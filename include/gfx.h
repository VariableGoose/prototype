#pragma once

#include <stdint.h>

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

#define color_arg(color) (color).r, (color).g, (color).b, (color).a

// -- Renderer -----------------------------------------------------------------
// Context containing everything neccessary to perform rendering.
typedef struct Renderer Renderer;

extern Renderer *renderer_new(uint32_t max_batch_size);
extern void renderer_free(Renderer *renderer);

extern void renderer_begin(Renderer *renderer);
extern void renderer_end(Renderer *renderer);
extern void renderer_submit(Renderer *renderer);

typedef struct Quad Quad;
struct Quad {
    int x, y;
    int w, h;
};

// Draws a quad onto the screen using screen coordinates.
// Origin in the top left corner.
extern void renderer_draw_quad(Renderer *renderer, Quad quad, Color color);
