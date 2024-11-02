#version 460 core

layout (location = 0) out vec4 frag_color;

in vec2 f_uv;
in vec4 f_color;
flat in uint f_texture_index;

uniform sampler2D textures[32];

void main() {
    frag_color = texture(textures[f_texture_index], f_uv) * f_color;
    // frag_color = f_color;
}
