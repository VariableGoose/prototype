#include "window.h"
#include "core.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

typedef struct KeyState KeyState;
struct KeyState {
    b8 first_press;
    b8 pressed;
};

struct Window {
    Allocator allocator;
    GLFWwindow *handle;
    Ivec2 size;

    KeyMod mods;
    KeyState keyboard[GLFW_KEY_LAST];
};

static void resize_callback(GLFWwindow* handle, int width, int height) {
    glViewport(0, 0, width, height);

    Window *window = glfwGetWindowUserPointer(handle);
    window->size = ivec2(width, height);
}

static void key_callback(GLFWwindow* handle, int key, int scancode, int action, int mods) {
    (void) scancode;
    Window *window = glfwGetWindowUserPointer(handle);
    window->mods = mods;
    switch (action) {
        case GLFW_PRESS:
            window->keyboard[key].pressed = true;
            window->keyboard[key].first_press = true;
            break;
        case GLFW_RELEASE:
            window->keyboard[key].pressed = false;
            window->keyboard[key].first_press = true;
            break;
    }
}

Window *window_new(u32 width, u32 height, const char *title, b8 resizable, Allocator allocator) {
    Window *window = allocator.alloc(sizeof(Window), allocator.ctx);

    *window = (Window) {
        .allocator = allocator,
        .size = ivec2(width, height),
    };

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, resizable);
    window->handle = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwSetFramebufferSizeCallback(window->handle, resize_callback);
    glfwSetKeyCallback(window->handle, key_callback);
    glfwSetWindowUserPointer(window->handle, window);
    glfwMakeContextCurrent(window->handle);

    return window;
}

void window_free(Window *window) {
    glfwDestroyWindow(window->handle);
    glfwTerminate();
    window->allocator.free(window, sizeof(Window), window->allocator.ctx);
}

void window_set_vsync(Window *window, b8 vsync) {
    (void) window;
    glfwSwapInterval(vsync);
}

void window_poll_event(Window *window) {
    for (u32 i = 0; i < arrlen(window->keyboard); i++) {
        window->keyboard[i].first_press = false;
    }
    glfwPollEvents();
}

void window_swap_buffers(Window *window) {
    glfwSwapBuffers(window->handle);
}

b8 window_is_open(const Window *window) {
    return !glfwWindowShouldClose(window->handle);
}

Ivec2 window_get_size(const Window *window) {
    return window->size;
}

KeyMod key_get_mods(const Window *window) {
    return window->mods;
}

b8 key_down(const Window *window, Key key) {
    return window->keyboard[key].pressed;
}

b8 key_up(const Window *window, Key key) {
    return !window->keyboard[key].pressed;
}

b8 key_press(const Window *window, Key key) {
    return window->keyboard[key].pressed && window->keyboard[key].first_press;
}

b8 key_release(const Window *window, Key key) {
    return !window->keyboard[key].pressed && window->keyboard[key].first_press;
}
