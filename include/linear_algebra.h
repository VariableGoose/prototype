#pragma once

#include "core.h"
#include <math.h>

typedef struct Vec2 Vec2;
struct Vec2 {
    f32 x, y;
};

static inline Vec2 vec2(f32 x, f32 y) { return (Vec2) {x, y}; }
static inline Vec2 vec2s(f32 scaler) { return (Vec2) {scaler, scaler}; }

static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return vec2(a.x+b.x, a.y+b.y); }
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return vec2(a.x-b.x, a.y-b.y); }
static inline Vec2 vec2_mul(Vec2 a, Vec2 b) { return vec2(a.x*b.x, a.y*b.y); }
static inline Vec2 vec2_div(Vec2 a, Vec2 b) { return vec2(a.x/b.x, a.y/b.y); }

static inline Vec2 vec2_adds(Vec2 vec, f32 scaler) { return vec2(vec.x+scaler, vec.y+scaler); }
static inline Vec2 vec2_subs(Vec2 vec, f32 scaler) { return vec2(vec.x-scaler, vec.y-scaler); }
static inline Vec2 vec2_muls(Vec2 vec, f32 scaler) { return vec2(vec.x*scaler, vec.y*scaler); }
static inline Vec2 vec2_divs(Vec2 vec, f32 scaler) { return vec2(vec.x/scaler, vec.y/scaler); }

static inline f32 vec2_magnitude_squared(Vec2 vec) {
    return vec.x*vec.x+vec.y*vec.y;
}

static inline f32 vec2_magnitude(Vec2 vec) {
    return sqrtf(vec.x*vec.x+vec.y*vec.y);
}

static inline Vec2 vec2_normalized(Vec2 vec) {
    f32 mag = vec2_magnitude(vec);
    if (mag == 0.0f) {
        return vec2s(0.0f);
    }
    f32 inv_mag = 1.0f/mag;
    return vec2_muls(vec, inv_mag);
}

static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t) {
    return vec2(lerp(a.x, b.x, t), lerp(a.y, b.y, t));
}

typedef struct Ivec2 Ivec2;
struct Ivec2 {
    i32 x, y;
};

static inline Ivec2 ivec2(i32 x, i32 y) { return (Ivec2) {x, y}; }
static inline Ivec2 ivec2s(i32 scaler) { return (Ivec2) {scaler, scaler}; }

static inline Ivec2 ivec2_add(Ivec2 a, Ivec2 b) { return ivec2(a.x+b.x, a.y+b.y); }
static inline Ivec2 ivec2_sub(Ivec2 a, Ivec2 b) { return ivec2(a.x-b.x, a.y-b.y); }
static inline Ivec2 ivec2_mul(Ivec2 a, Ivec2 b) { return ivec2(a.x*b.x, a.y*b.y); }
static inline Ivec2 ivec2_div(Ivec2 a, Ivec2 b) { return ivec2(a.x/b.x, a.y/b.y); }

static inline Ivec2 ivec2_adds(Ivec2 vec, i32 scaler) { return ivec2(vec.x+scaler, vec.y+scaler); }
static inline Ivec2 ivec2_subs(Ivec2 vec, i32 scaler) { return ivec2(vec.x-scaler, vec.y-scaler); }
static inline Ivec2 ivec2_muls(Ivec2 vec, i32 scaler) { return ivec2(vec.x*scaler, vec.y*scaler); }
static inline Ivec2 ivec2_divs(Ivec2 vec, i32 scaler) { return ivec2(vec.x/scaler, vec.y/scaler); }

#define vec2_arg(vec) (vec).x, (vec).y
