#include "core.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

void *malloc_stub(u64 size, void *ctx) {
    (void) ctx;
    return malloc(size);
}

void free_stub(void *ptr, u64 size, void *ctx) {
    (void) ctx;
    (void) size;
    return free(ptr);
}

void *realloc_stub(void *ptr, u64 old, u64 new, void *ctx) {
    (void) ctx;
    (void) old;
    return realloc(ptr, new);
}

void _log(LogLevel level, const char *file, u32 line, const char *fmt, ...) {
    const char *level_prefix[] = {
        "\033[0;101m[FATAL]",
        "\033[0;91m[ERROR]",
        "\033[0;93m[WARN] ",
        "\033[0;92m[INFO] ",
        "\033[0;94m[DEBUG]",
        "\033[0;95m[TRACE]"
    };

    char msg[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, arrlen(msg), fmt, args);
    va_end(args);

    printf("%s %s:%u: %s\033[0;0m\n", level_prefix[level], file, line, msg);
}

// AABB
Vec2 aabb_half_size(AABB aabb) {
    return vec2_divs(aabb.size, 2.0f);
}

b8 aabb_overlap_point(AABB aabb, Vec2 point) {
    Vec2 half = aabb_half_size(aabb);
    f32 l = aabb.position.x - half.x;
    f32 r = aabb.position.x + half.x;
    f32 b = aabb.position.y - half.y;
    f32 t = aabb.position.y + half.y;
    return point.x >= l && point.x <= r && point.y >= b && point.y <= t;
}

b8 aabb_overlap_aabb(AABB a, AABB b) {
    Vec2 a_half = aabb_half_size(a);
    Vec2 b_half = aabb_half_size(b);

    Vec2 a_min = vec2_sub(a.position, a_half);
    Vec2 a_max = vec2_add(a.position, a_half);
    Vec2 b_min = vec2_sub(b.position, b_half);
    Vec2 b_max = vec2_add(b.position, b_half);

    return a_min.x <= b_max.x &&
        a_max.x >= b_min.x &&
        a_min.y <= b_max.y &&
        a_max.y >= b_min.y;
}

b8 aabb_overlap_circle(AABB aabb, Vec2 circle_position, f32 circle_radius) {
    // https://www.jeffreythompson.org/collision-detection/circle-rect.php
    Vec2 half_size = aabb_half_size(aabb);

    Vec2 min = vec2_sub(aabb.position, half_size);
    Vec2 max = vec2_add(aabb.position, half_size);

    Vec2 test_pos = circle_position;

    // Find the closest edge.
    if (circle_position.x < min.x) { test_pos.x = min.x; }
    else if (circle_position.x > max.x) { test_pos.x = max.x; }
    if (circle_position.y < min.y) { test_pos.y = min.y; }
    else if (circle_position.y > max.y) { test_pos.y = max.y; }

    // Get distance from closest edge.
    Vec2 diff = vec2_sub(circle_position, test_pos);
    f32 dist = vec2_magnitude_squared(diff);

    return dist <= circle_radius*circle_radius;
}

MinkowskiDifference aabb_minkowski_difference(AABB a, AABB b) {
    // https://blog.hamaluik.ca/posts/simple-aabb-collision-using-minkowski-difference/
    MinkowskiDifference diff = {0};

    Vec2 a_half = aabb_half_size(a);
    Vec2 b_half = aabb_half_size(b);

    Vec2 a_min = vec2_sub(a.position, a_half);
    Vec2 b_max = vec2_add(b.position, b_half);
    Vec2 size = vec2_add(a.size, b.size);
    Vec2 min = vec2_sub(a_min, b_max);
    Vec2 max = vec2_add(min, size);
    diff.is_overlapping = (
            min.x < 0.0f &&
            max.x > 0.0f &&
            min.y < 0.0f &&
            max.y > 0.0f
        );

    if (!diff.is_overlapping) {
        return diff;
    }

    f32 min_dist = fabsf(-min.x);
    Vec2 bound_point = vec2(min.x, 0.0f);
    if (fabsf(max.x) < min_dist) {
        min_dist = fabsf(max.x);
        bound_point = vec2(max.x, 0.0f);
    }
    if (fabsf(max.y) < min_dist) {
        min_dist = fabsf(max.y);
        bound_point = vec2(0.0f, max.y);
    }
    if (fabsf(min.y) < min_dist) {
        min_dist = fabsf(min.y);
        bound_point = vec2(0.0f, min.y);
    }
    diff.depth = bound_point;

    if (fabsf(diff.depth.x) > fabsf(diff.depth.y)) {
        if (diff.depth.x > 0.0f) {
            diff.normal.x = 1.0f;
        } else {
            diff.normal.x = -1.0f;
        }
    } else {
        if (diff.depth.y > 0.0f) {
            diff.normal.y = 1.0f;
        } else {
            diff.normal.y = -1.0f;
        }
    }

    return diff;
}
