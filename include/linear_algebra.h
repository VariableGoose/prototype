#pragma once

typedef struct Vec2 Vec2;
struct Vec2 {
    float x, y;
};

static inline Vec2 vec2(float x, float y) { return (Vec2) {x, y}; }
static inline Vec2 vec2s(float scaler) { return (Vec2) {scaler, scaler}; }

typedef struct Ivec2 Ivec2;
struct Ivec2 {
    int x, y;
};

static inline Ivec2 ivec2(int x, int y) { return (Ivec2) {x, y}; }
static inline Ivec2 ivec2s(int scaler) { return (Ivec2) {scaler, scaler}; }

static inline Ivec2 ivec2_add(Ivec2 a, Ivec2 b) { return ivec2(a.x+b.x, a.y+b.y); }

#define vec2_arg(vec) (vec).x, (vec).y
