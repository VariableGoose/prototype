#include "gfx.h"
#include "internal.h"

#include <glad/gl.h>
#include <stdio.h>

Texture texture_new(Renderer *renderer, TextureDesc desc) {
    TextureInternal internal = {
        .size = ivec2(desc.width, desc.height),
    };

    uint32_t gl_internal_format = 0;
    uint32_t gl_format = 0;
    uint32_t gl_type = 0;
    switch (desc.data_type) {
        case TEXTURE_DATA_TYPE_BYTE:
            gl_type = GL_BYTE;
            break;
        case TEXTURE_DATA_TYPE_UBYTE:
            gl_type = GL_UNSIGNED_BYTE;
            break;

        case TEXTURE_DATA_TYPE_INT:
            gl_type = GL_INT;
            break;
        case TEXTURE_DATA_TYPE_UINT:
            gl_type = GL_UNSIGNED_INT;
            break;

        case TEXTURE_DATA_TYPE_FLOAT16:
            gl_type = GL_FLOAT;
            break;
        case TEXTURE_DATA_TYPE_FLOAT32:
            gl_type = GL_FLOAT;
            break;
    }

    switch (desc.format) {
        case TEXTURE_FORMAT_R:
            gl_internal_format = GL_RED;
            gl_format = GL_RED;
            break;
        case TEXTURE_FORMAT_RG:
            gl_internal_format = GL_RG;
            gl_format = GL_RG;
            break;
        case TEXTURE_FORMAT_RGB:
            gl_internal_format = GL_RGB;
            gl_format = GL_RGB;
            break;
        case TEXTURE_FORMAT_RGBA:
            gl_internal_format = GL_RGBA;
            gl_format = GL_RGBA;
            break;
    }

    switch (desc.data_type) {
        case TEXTURE_DATA_TYPE_FLOAT16:
            switch (desc.format) {
                case TEXTURE_FORMAT_R:
                    gl_internal_format = GL_R16F;
                    break;
                case TEXTURE_FORMAT_RG:
                    gl_internal_format = GL_RG16F;
                    break;
                case TEXTURE_FORMAT_RGB:
                    gl_internal_format = GL_RGB16F;
                    break;
                case TEXTURE_FORMAT_RGBA:
                    gl_internal_format = GL_RGBA16F;
                    break;
            }
            break;
        case TEXTURE_DATA_TYPE_FLOAT32:
            switch (desc.format) {
                case TEXTURE_FORMAT_R:
                    gl_internal_format = GL_R32F;
                    break;
                case TEXTURE_FORMAT_RG:
                    gl_internal_format = GL_RG32F;
                    break;
                case TEXTURE_FORMAT_RGB:
                    gl_internal_format = GL_RGB32F;
                    break;
                case TEXTURE_FORMAT_RGBA:
                    gl_internal_format = GL_RGBA32F;
                    break;
            }
            break;
        default:
            break;
    }

    // TODO: Add mipmap support.
    uint32_t gl_sampler = 0;
    switch (desc.sampler) {
        case TEXTURE_SAMPLER_LINEAR:
            gl_sampler = GL_LINEAR;
            break;
        case TEXTURE_SAMPLER_NEAREST:
            gl_sampler = GL_NEAREST;
            break;
    }

    // printf("type: %x\n", gl_type);
    // printf("format: %x\n", gl_format);
    // printf("internal format: %x\n", gl_internal_format);

    glGenTextures(1, &internal.id);
    glBindTexture(GL_TEXTURE_2D, internal.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_sampler);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_sampler);

    // TODO: Make textures work with only three channels as well.
    glTexImage2D(GL_TEXTURE_2D,
            0,
            gl_internal_format,
            desc.width,
            desc.height,
            0,
            gl_format,
            gl_type,
            desc.pixels);

    // for (size_t j = 0; j < desc.width*desc.height; j++) {
    //     for (size_t i = 0; i < desc.format; i++) {
    //         printf("%u ", ((uint8_t *) desc.pixels)[j*desc.format+i]);
    //     }
    //     printf("\n");
    // }

    glBindTexture(GL_TEXTURE_2D, 0);

    vec_push(renderer->textures, internal);
    return vec_len(renderer->textures)-1;
}

Ivec2 texture_get_size(const Renderer *renderer, Texture texture) {
    return renderer->textures[texture].size;
}
