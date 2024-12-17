#include "gfx.h"
#include <stdio.h>

Vec2 screen_to_world_space(Camera camera, Vec2 screen) {
    screen = vec2_sub(screen, vec2(vec2_arg(ivec2_divs(camera.screen_size, 2))));
    Vec4 normalized = vec4((screen.x*2.0f)/camera.screen_size.x, screen.y*2.0f/camera.screen_size.y, 0.0f, 1.0f);

    f32 aspect = (f32) camera.screen_size.x / (f32) camera.screen_size.y;
    f32 zoom = camera.zoom / 2.0f;
    Mat4 projection = mat4_inv_ortho_projection(-aspect*zoom, aspect*zoom, zoom, -zoom, 1.0f, -1.0f);
    Vec4 world_v4 = mat4_mul_vec(projection, normalized);
    Vec2 world = vec2(vec2_arg(world_v4));
    world = vec2_mul(world, camera.direction);
    world = vec2_mul(world, vec2(1.0f, -1.0));
    world = vec2_add(world, camera.position);
    // printf("(%f, %f) -> (%f, %f)\n", vec2_arg(screen), vec2_arg(world));
    return vec2(vec2_arg(world));
}

Vec2 world_to_screen_space(Camera camera, Vec2 world) {
    f32 aspect = (f32) camera.screen_size.x / (f32) camera.screen_size.y;
    f32 zoom = camera.zoom / 2.0f;
    world = vec2_sub(world, camera.position);
    Mat4 projection = mat4_ortho_projection(-aspect*zoom, aspect*zoom, zoom, -zoom, 1.0f, -1.0f);
    Vec4 normalized = mat4_mul_vec(projection, vec4(world.x, world.y, 0.0f, 1.0f));
    Vec2 screen = vec2(normalized.x*camera.screen_size.x/2.0f, normalized.y*camera.screen_size.y/2.0f);
    screen = vec2_add(screen, vec2(vec2_arg(ivec2_divs(camera.screen_size, 2))));
    // printf("(%f, %f) -> (%f, %f)\n", vec2_arg(world), vec2_arg(screen));
    return screen;
}
