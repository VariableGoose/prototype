#include "window.h"
#include "core.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

struct Window {
    Allocator allocator;
    GLFWwindow *handle;
    Ivec2 size;
};

void resize_callback(GLFWwindow* handle, int width, int height) {
    glViewport(0, 0, width, height);
    Window *window = glfwGetWindowUserPointer(handle);
    window->size = ivec2(width, height);
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
    glfwSetWindowUserPointer(window->handle, window);
    // glfwSwapInterval(0);
    glfwMakeContextCurrent(window->handle);

    return window;
}

void window_free(Window *window) {
    glfwDestroyWindow(window->handle);
    glfwTerminate();
    window->allocator.free(window, sizeof(Window), window->allocator.ctx);
}

void window_poll_event(void) {
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
