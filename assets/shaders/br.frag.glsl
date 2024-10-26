#version 330 core

layout (location = 0) out vec4 frag_color;

in vec2 f_uv;
in vec4 f_color;

void main() {
    // frag_color = vec4(f_uv, 0.0, 1.0);
    frag_color = f_color;
}
