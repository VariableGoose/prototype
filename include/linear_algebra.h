#pragma once

typedef struct Vec2 Vec2;
struct Vec2 {
    float x, y;
};

static inline Vec2 vec2(float x, float y) { return (Vec2) {x, y}; }
static inline Vec2 vec2s(float scaler) { return (Vec2) {scaler, scaler}; }
