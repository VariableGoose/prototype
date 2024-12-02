#pragma once

#include "core.h"

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

typedef struct Ivec2 Ivec2;
struct Ivec2 {
    i32 x, y;
};

static inline Ivec2 ivec2(i32 x, i32 y) { return (Ivec2) {x, y}; }
static inline Ivec2 ivec2s(i32 scaler) { return (Ivec2) {scaler, scaler}; }

static inline Ivec2 ivec2_add(Ivec2 a, Ivec2 b) { return ivec2(a.x+b.x, a.y+b.y); }

#define vec2_arg(vec) (vec).x, (vec).y
