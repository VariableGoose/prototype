#include "window.h"
#include "core.h"
#include <stdio.h>

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
    KeyState keyboard[KEY_COUNT];
    struct {
        Vec2 pos;
        KeyState button[MOUSE_BUTTON_COUNT];
    } mouse;
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

static void cursor_pos_callback(GLFWwindow* handle, double xpos, double ypos) {
    Window *window = glfwGetWindowUserPointer(handle);
    window->mouse.pos = vec2(xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* handle, int button, int action, int mods) {
    Window *window = glfwGetWindowUserPointer(handle);
    window->mods = mods;
    switch (action) {
        case GLFW_PRESS:
            window->mouse.button[button].pressed = true;
            window->mouse.button[button].first_press = true;
            break;
        case GLFW_RELEASE:
            window->mouse.button[button].pressed = false;
            window->mouse.button[button].first_press = true;
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
    glfwSetWindowUserPointer(window->handle, window);

    glfwSetFramebufferSizeCallback(window->handle, resize_callback);
    glfwSetKeyCallback(window->handle, key_callback);
    glfwSetCursorPosCallback(window->handle, cursor_pos_callback);
    glfwSetMouseButtonCallback(window->handle, mouse_button_callback);

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
    for (u32 i = 0; i < arrlen(window->mouse.button); i++) {
        window->mouse.button[i].first_press = false;
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

Vec2 mouse_position(const Window *window) {
    return window->mouse.pos;
}

b8 mouse_button_down(const Window *window, MouseButton button) {
    return window->mouse.button[button].pressed;
}

b8 mouse_button_up(const Window *window, MouseButton button) {
    return !window->mouse.button[button].pressed;
}

b8 mouse_button_press(const Window *window, MouseButton button) {
    return window->mouse.button[button].pressed && window->mouse.button[button].first_press;
}

b8 mouse_button_release(const Window *window, MouseButton button) {
    return !window->mouse.button[button].pressed && window->mouse.button[button].first_press;
}
