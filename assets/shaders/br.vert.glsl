#version 330 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 color;

out vec2 f_uv;
out vec4 f_color;

void main() {
    f_uv = uv;
    f_color = color;

    gl_Position = vec4(pos, 0.0, 1.0);
}
