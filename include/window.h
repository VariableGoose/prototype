#pragma once

#include "core.h"
#include "linear_algebra.h"

typedef struct Window Window;

extern Window *window_new(u32 width, u32 height, const char *title, b8 resizable, Allocator allocator);
extern void window_free(Window *window);

extern void window_poll_event(void);
extern void window_swap_buffers(Window *window);

extern b8 window_is_open(const Window *window);
extern Ivec2 window_get_size(const Window *window);
