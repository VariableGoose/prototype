#version 330 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 color;
layout (location = 3) in uint texture_index;

out vec2 f_uv;
out vec4 f_color;
flat out uint f_texture_index;

struct Camera {
    ivec2 screen_size;
    vec2 pos;
    float zoom;
    vec2 dir;
};
uniform Camera cam;

void main() {
    f_uv = uv;
    f_color = color;
    f_texture_index = texture_index;

    vec2 screen_pos = pos;
    screen_pos -= cam.pos*cam.dir;
    float aspect = float(cam.screen_size.x)/float(cam.screen_size.y);
    screen_pos /= vec2(aspect*cam.zoom, cam.zoom)/2;
    gl_Position = vec4(screen_pos, 0.0, 1.0);
}
