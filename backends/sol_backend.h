/*
 * SolsticeSFG - Cross-OS Platform Backend Interface (Phase 7.1)
 * SPDX-License-Identifier: Zlib OR MIT
 */

#ifndef SOL_BACKEND_H
#define SOL_BACKEND_H

#include "solsticesfg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sol_event_type {
    SOL_EVENT_NONE = 0,
    SOL_EVENT_QUIT,
    SOL_EVENT_RESIZE,
    SOL_EVENT_MOUSE_MOVE,
    SOL_EVENT_MOUSE_DOWN,
    SOL_EVENT_MOUSE_UP,
    SOL_EVENT_MOUSE_WHEEL,
    SOL_EVENT_KEY_DOWN,
    SOL_EVENT_KEY_UP
} sol_event_type_t;

typedef enum sol_mouse_btn {
    SOL_MOUSE_NONE = 0,
    SOL_MOUSE_LEFT = 1,
    SOL_MOUSE_RIGHT = 2,
    SOL_MOUSE_MIDDLE = 3
} sol_mouse_btn_t;

typedef struct sol_event {
    sol_event_type_t type;
    int32_t x;
    int32_t y;
    int32_t wheel_delta; /* +1 up, -1 down */
    sol_mouse_btn_t button;
    int32_t width;
    int32_t height;
    uint32_t keycode; /* ASCII / OS virtual key for sandbox shortcuts */
} sol_event_t;

typedef struct sol_window sol_window_t;

sol_window_t *sol_window_create(const char *title, int32_t width, int32_t height);
void sol_window_destroy(sol_window_t *win);
void sol_window_set_title(sol_window_t *win, const char *title);

bool sol_window_poll_event(sol_window_t *win, sol_event_t *out_ev);
sol_surface_t *sol_window_surface(sol_window_t *win);
void sol_window_present(sol_window_t *win);
void sol_window_present_rect(sol_window_t *win, sol_rect_t dirty);

uint64_t sol_time_ms(void);
void sol_sleep_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* SOL_BACKEND_H */
