#include <stdio.h>

#include "sol_backend.h"

static void draw_topbar(sol_surface_t *surf, int32_t active_tab, int32_t mouse_x, int32_t mouse_y) {
    int32_t i;
    sol_color_t tab_colors[4] = {SOL_RGB(90, 140, 255), SOL_RGB(255, 110, 120),
                                 SOL_RGB(70, 210, 160), SOL_RGB(255, 185, 60)};

    sol_fill_rect(surf, SOL_RECT(0, 0, surf->width, 52), SOL_ARGB(220, 18, 20, 32));
    sol_draw_hline(surf, 0, 52, surf->width, SOL_ARGB(90, 255, 255, 255));

    for (i = 0; i < 4; ++i) {
        sol_rect_t tab = SOL_RECT(16 + i * 116, 10, 104, 32);
        bool hovered = sol_rect_contains_point(tab, mouse_x, mouse_y);
        bool active = (i == active_tab);
        sol_color_t bg =
            active ? tab_colors[i] : (hovered ? SOL_RGB(52, 58, 84) : SOL_RGB(34, 38, 56));

        sol_fill_rounded_rect(surf, tab, 8, bg);
        if (active) {
            sol_draw_rounded_rect(surf, tab, 8, SOL_COLOR_WHITE);
        }

        /* Tiny geometric icon inside each tab so tabs are self-explanatory without text yet */
        if (i == 0) {
            sol_fill_circle(surf, tab.x + 52, tab.y + 16, 7, SOL_COLOR_WHITE);
        } else if (i == 1) {
            sol_fill_circle(surf, tab.x + 47, tab.y + 16, 6, SOL_ARGB(190, 255, 255, 255));
            sol_fill_circle(surf, tab.x + 57, tab.y + 16, 6, SOL_ARGB(190, 255, 255, 255));
        } else if (i == 2) {
            sol_fill_rect_gradient_h(surf, SOL_RECT(tab.x + 34, tab.y + 10, 36, 12),
                                     SOL_COLOR_WHITE, SOL_RGB(40, 40, 80));
        } else {
            sol_draw_rect(surf, SOL_RECT(tab.x + 40, tab.y + 9, 24, 14), SOL_COLOR_WHITE);
            sol_fill_rect(surf, SOL_RECT(tab.x + 45, tab.y + 13, 14, 6), SOL_COLOR_WHITE);
        }
    }
}

static void render_scene_shapes(sol_surface_t *surf, int32_t mx, int32_t my, int32_t radius,
                                uint64_t tick) {
    int32_t w = surf->width;
    int32_t h = surf->height;
    int32_t pulse = (int32_t)((tick / 20u) % 60u);
    int32_t anim = (pulse < 30) ? pulse : (60 - pulse);

    sol_fill_rect_gradient_v(surf, SOL_RECT(0, 53, w, h - 53), SOL_RGB(16, 18, 34),
                             SOL_RGB(38, 22, 54));

    sol_fill_rounded_rect(surf, SOL_RECT(24, 72, 270, 170), 16, SOL_ARGB(200, 28, 34, 54));
    sol_draw_rounded_rect(surf, SOL_RECT(24, 72, 270, 170), 16, SOL_RGB(110, 140, 255));

    sol_fill_rect_gradient_h(surf, SOL_RECT(44, 92, 230, 24), SOL_RGB(255, 95, 109),
                             SOL_RGB(255, 195, 113));
    sol_fill_rect(surf, SOL_SQUARE(44, 134, 64), SOL_RGB(70, 200, 160));
    sol_draw_rect_thick(surf, SOL_SQUARE(128, 134, 64), 4, SOL_COLOR_WHITE);

    /* Animated RGB Venn circles */
    sol_fill_circle(surf, 460 - anim / 2, 145, 62, SOL_ARGB(170, 255, 70, 90));
    sol_fill_circle(surf, 520 + anim / 2, 145, 62, SOL_ARGB(170, 70, 230, 140));
    sol_fill_circle(surf, 490, 200 + anim / 3, 62, SOL_ARGB(170, 70, 140, 255));

    sol_fill_triangle(surf, 90, 390, 190, 265, 290, 390, SOL_ARGB(220, 255, 165, 40));
    sol_draw_triangle(surf, 90, 390, 190, 265, 290, 390, SOL_COLOR_WHITE);

    sol_fill_ellipse(surf, 490, 345, 115, 46, SOL_ARGB(180, 40, 190, 230));
    sol_draw_ellipse(surf, 490, 345, 115, 46, SOL_COLOR_CYAN);

    /* Interactive cursor circle + crosshairs */
    if (my > 52) {
        sol_fill_circle(surf, mx, my, radius, SOL_ARGB(90, 255, 255, 255));
        sol_draw_circle(surf, mx, my, radius, SOL_ARGB(200, 255, 220, 100));
        sol_draw_line(surf, mx - 12, my, mx + 12, my, SOL_COLOR_WHITE);
        sol_draw_line(surf, mx, my - 12, mx, my + 12, SOL_COLOR_WHITE);
    }
}

static void render_scene_blending(sol_surface_t *surf, int32_t mx, int32_t my, int32_t radius) {
    sol_blend_mode_t modes[5] = {SOL_BLEND_SRC_OVER, SOL_BLEND_ADD, SOL_BLEND_MULTIPLY,
                                 SOL_BLEND_SCREEN, SOL_BLEND_OVERLAY};
    int32_t i;

    sol_fill_rect_gradient_4(surf, SOL_RECT(0, 53, surf->width, surf->height - 53),
                             SOL_RGB(40, 70, 180), SOL_RGB(220, 60, 110), SOL_RGB(250, 200, 60),
                             SOL_RGB(30, 180, 140));

    for (i = 0; i < 5; ++i) {
        sol_rect_t box = SOL_RECT(24 + i * 146, 110, 130, 240);
        sol_fill_rect_blend(surf, box, SOL_ARGB(180, 110, 190, 255), modes[i]);
        sol_draw_rect(surf, box, SOL_COLOR_WHITE);
    }

    if (my > 52) {
        sol_fill_rect_blend(surf, SOL_RECT(mx - radius, my - radius, radius * 2, radius * 2),
                            SOL_ARGB(180, 255, 120, 60), SOL_BLEND_ADD);
        sol_draw_rect(surf, SOL_RECT(mx - radius, my - radius, radius * 2, radius * 2),
                      SOL_COLOR_WHITE);
    }
}

static void render_scene_gradients(sol_surface_t *surf, int32_t mx, int32_t my, int32_t radius) {
    sol_gradient_stop_t sunset[4] = {{0u, SOL_RGB(25, 15, 55)},
                                     {90u, SOL_RGB(170, 45, 110)},
                                     {180u, SOL_RGB(255, 130, 60)},
                                     {255u, SOL_RGB(255, 235, 140)}};
    sol_gradient_stop_t glow[3] = {{0u, SOL_ARGB(240, 255, 255, 220)},
                                   {100u, SOL_ARGB(180, 90, 180, 255)},
                                   {255u, SOL_ARGB(0, 20, 20, 40)}};

    sol_fill_rect_gradient_linear(surf, SOL_RECT(24, 76, 350, 220), 24, 76, 374, 296, sunset, 4u,
                                  true);
    sol_draw_rect(surf, SOL_RECT(24, 76, 350, 220), SOL_COLOR_WHITE);

    sol_fill_rect_gradient_4(surf, SOL_RECT(400, 76, 350, 220), SOL_COLOR_RED, SOL_COLOR_GREEN,
                             SOL_COLOR_WHITE, SOL_COLOR_BLUE);
    sol_draw_rect(surf, SOL_RECT(400, 76, 350, 220), SOL_COLOR_WHITE);

    /* Radial gradient spotlight tracking the mouse */
    sol_fill_rect_gradient_radial(surf, SOL_RECT(24, 316, surf->width - 48, surf->height - 340), mx,
                                  my, radius * 3, glow, 3u, false);
    sol_draw_rect(surf, SOL_RECT(24, 316, surf->width - 48, surf->height - 340),
                  SOL_ARGB(140, 255, 255, 255));
}

static void render_scene_subviews(sol_surface_t *surf, uint64_t tick) {
    sol_surface_t panel;
    sol_clip_stack_t stack;
    int32_t anim_x = (int32_t)((tick / 6u) % 420u) - 40;

    sol_fill_rect(surf, SOL_RECT(0, 53, surf->width, surf->height - 53), SOL_RGB(20, 24, 32));

    if (sol_surface_subview(&panel, surf, SOL_RECT(60, 90, 340, 280)) == SOL_OK) {
        sol_clear(&panel, SOL_RGB(34, 42, 60));
        sol_clip_stack_init(&stack);

        sol_push_clip(&panel, &stack, SOL_RECT(20, 20, 300, 240));
        sol_fill_rect(&panel, SOL_RECT(0, 0, 340, 280), SOL_RGB(48, 60, 86));
        sol_fill_circle(&panel, anim_x, 120, 65, SOL_ARGB(210, 255, 110, 90));
        sol_fill_rounded_rect(&panel, SOL_RECT(140, 90, 180, 180), 20, SOL_ARGB(180, 70, 220, 160));
        sol_pop_clip(&panel, &stack);

        sol_draw_rect(&panel, SOL_RECT(0, 0, panel.width, panel.height), SOL_COLOR_CYAN);
    }

    if (sol_surface_subview(&panel, surf, SOL_RECT(430, 90, 310, 280)) == SOL_OK) {
        sol_clear(&panel, SOL_RGB(48, 30, 52));
        sol_fill_triangle(&panel, anim_x, 40, 280, 240, 30, 220, SOL_RGB(255, 190, 60));
        sol_draw_rect(&panel, SOL_RECT(0, 0, panel.width, panel.height), SOL_COLOR_MAGENTA);
    }
}

int main(void) {
    sol_window_t *win = sol_window_create("SolsticeSFG Sandbox", 800, 520);
    sol_event_t ev;
    bool running = true;
    int32_t active_tab = 0;
    int32_t mouse_x = 400;
    int32_t mouse_y = 260;
    int32_t radius = 48;
    uint64_t start_ms = sol_time_ms();
    uint64_t fps_timer = start_ms;
    uint32_t frames = 0;

    if (win == NULL) {
        fprintf(stderr, "Failed to open SolsticeSFG window\n");
        return 1;
    }

    while (running) {
        while (sol_window_poll_event(win, &ev)) {
            switch (ev.type) {
            case SOL_EVENT_QUIT:
                running = false;
                break;
            case SOL_EVENT_MOUSE_MOVE:
                mouse_x = ev.x;
                mouse_y = ev.y;
                break;
            case SOL_EVENT_MOUSE_DOWN:
                if (ev.button == SOL_MOUSE_LEFT && ev.y >= 10 && ev.y <= 42) {
                    int32_t i;
                    for (i = 0; i < 4; ++i) {
                        if (sol_rect_contains_point(SOL_RECT(16 + i * 116, 10, 104, 32), ev.x,
                                                    ev.y)) {
                            active_tab = i;
                        }
                    }
                }
                break;
            case SOL_EVENT_MOUSE_WHEEL:
                radius += ev.wheel_delta * 6;
                if (radius < 12) {
                    radius = 12;
                }
                if (radius > 160) {
                    radius = 160;
                }
                break;
            case SOL_EVENT_KEY_DOWN:
                if (ev.keycode == 27u) { /* ESC */
                    running = false;
                } else if (ev.keycode >= '1' && ev.keycode <= '4') {
                    active_tab = (int32_t)(ev.keycode - '1');
                }
                break;
            default:
                break;
            }
        }

        {
            sol_surface_t *surf = sol_window_surface(win);
            uint64_t now = sol_time_ms();
            uint64_t tick = now - start_ms;

            if (surf != NULL) {
                if (active_tab == 0) {
                    render_scene_shapes(surf, mouse_x, mouse_y, radius, tick);
                } else if (active_tab == 1) {
                    render_scene_blending(surf, mouse_x, mouse_y, radius);
                } else if (active_tab == 2) {
                    render_scene_gradients(surf, mouse_x, mouse_y, radius);
                } else {
                    render_scene_subviews(surf, tick);
                }
                draw_topbar(surf, active_tab, mouse_x, mouse_y);
                sol_window_present(win);
            }

            frames++;
            if (now - fps_timer >= 500u) {
                char title[128];
                uint32_t fps = (uint32_t)((frames * 1000u) / (now - fps_timer));
                snprintf(title, sizeof(title),
                         "SolsticeSFG Sandbox | Scene %d (keys 1-4, wheel=size, ESC=quit) | %u FPS",
                         active_tab + 1, fps);
                sol_window_set_title(win, title);
                frames = 0;
                fps_timer = now;
            }
        }

        sol_sleep_ms(8u);
    }

    sol_window_destroy(win);
    return 0;
}
