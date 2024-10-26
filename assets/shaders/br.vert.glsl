#version 330 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 color;

out vec2 f_uv;
out vec4 f_color;

uniform ivec2 screen_size;

void main() {
    f_uv = uv;
    f_color = color;

    vec2 half_screen = screen_size/2.0;
    vec2 screen_pos = pos;
    screen_pos -= vec2(half_screen.x, half_screen.y);
    screen_pos.y = -screen_pos.y;
    screen_pos /= half_screen;
    gl_Position = vec4(screen_pos, 0.0, 1.0);
}
