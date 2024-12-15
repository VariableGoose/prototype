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

static inline f32 vec2_dot(Vec2 a, Vec2 b) {
    return a.x*b.x + a.y*b.y;
}

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

typedef struct Vec4 Vec4;
struct Vec4 {
    f32 x, y, z, w;
};

static inline Vec4 vec4(f32 x, f32 y, f32 z, f32 w) { return (Vec4) {x, y, z, w}; }
static inline Vec4 vec4s(f32 scaler) { return (Vec4) {scaler, scaler, scaler, scaler}; }

typedef struct Mat4 Mat4;
struct Mat4 {
    Vec4 a, b, c, d;
};

static const Mat4 MAT4_IDENTITY = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

static inline Vec4 mat4_mul_vec(Mat4 mat, Vec4 vec) {
    return (Vec4) {
        vec.x*mat.a.x + vec.y*mat.a.y + vec.z*mat.a.z + vec.w*mat.a.w,
        vec.x*mat.b.x + vec.y*mat.b.y + vec.z*mat.b.z + vec.w*mat.b.w,
        vec.x*mat.c.x + vec.y*mat.c.y + vec.z*mat.c.z + vec.w*mat.c.w,
        vec.x*mat.d.x + vec.y*mat.d.y + vec.z*mat.d.z + vec.w*mat.d.w,
    };
}

// https://en.wikipedia.org/wiki/Orthographic_projection#Geometry
static inline Mat4 mat4_ortho_projection(f32 left, f32 right, f32 top, f32 bottom, f32 far, f32 near) {
    f32 x = 2.0f / (right - left);
    f32 y = 2.0f / (top - bottom);
    f32 z = -2.0f / (far - near);

    f32 x_off = -(right+left) / (right-left);
    f32 y_off = -(top+bottom) / (top-bottom);
    f32 z_off = -(far+near) / (far-near);

    return (Mat4) {
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    };
}

static inline Mat4 mat4_inv_ortho_projection(f32 left, f32 right, f32 top, f32 bottom, f32 far, f32 near) {
    f32 x = (right - left) / 2.0f;
    f32 y = (top - bottom) / 2.0f;
    f32 z = (far - near) / -2.0f;

    f32 x_off = (left+right) / 2.0f;
    f32 y_off = (top+bottom) / 2.0f;
    f32 z_off = -(far+near) / 2.0f;

    return (Mat4) {
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    };
}
