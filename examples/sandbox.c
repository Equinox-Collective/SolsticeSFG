#include <stdio.h>

#include "sol_backend.h"

#define AIM_TARGET_SIZE 96
#define AIM_MAX_WAVES 10

typedef struct aim_wave {
    bool active;
    bool is_hit;
    int32_t x;
    int32_t y;
    uint64_t start_ms;
} aim_wave_t;

typedef struct aim_state {
    bool initialized;
    uint32_t rng;
    int32_t pos_x_fp8;
    int32_t pos_y_fp8;
    int32_t vel_x_fp8;
    int32_t vel_y_fp8;
    uint64_t next_change_ms;
    uint64_t next_jump_ms;
    uint32_t hover_blend; /* 0..255 */
    uint32_t hits;
    uint32_t misses;
    aim_wave_t waves[AIM_MAX_WAVES];

    uint32_t normal_px[AIM_TARGET_SIZE * AIM_TARGET_SIZE];
    uint32_t green_px[AIM_TARGET_SIZE * AIM_TARGET_SIZE];
    uint32_t work_px[AIM_TARGET_SIZE * AIM_TARGET_SIZE];
    sol_surface_t normal_surf;
    sol_surface_t green_surf;
    sol_surface_t work_surf;
} aim_state_t;

static uint32_t aim_rand(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0u) {
        x = 0x9E3779B9u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int32_t aim_rand_range(uint32_t *state, int32_t lo, int32_t hi) {
    if (hi <= lo) {
        return lo;
    }
    return lo + (int32_t)(aim_rand(state) % (uint32_t)(hi - lo + 1));
}

static void aim_build_fallback_png(sol_image_t *out_img) {
    enum { SRC_SZ = 128 };
    uint32_t tmp_px[SRC_SZ * SRC_SZ];
    uint8_t png_buf[SRC_SZ * SRC_SZ * 5 + 256];
    sol_surface_t tmp_surf;
    size_t png_len = 0u;

    sol_surface_init(&tmp_surf, tmp_px, SRC_SZ, SRC_SZ, SRC_SZ);
    sol_fill_rect_gradient_4(&tmp_surf, SOL_RECT(0, 0, SRC_SZ, SRC_SZ), SOL_RGB(255, 80, 90),
                             SOL_RGB(255, 200, 60), SOL_RGB(70, 140, 255), SOL_RGB(190, 70, 240));
    sol_fill_circle_aa(&tmp_surf, 64, 64, 52, SOL_ARGB(220, 25, 28, 45));
    sol_fill_circle_aa(&tmp_surf, 64, 64, 38, SOL_RGB(255, 110, 70));
    sol_fill_circle_aa(&tmp_surf, 64, 64, 22, SOL_COLOR_WHITE);
    sol_fill_circle_aa(&tmp_surf, 64, 64, 10, SOL_RGB(255, 60, 80));
    sol_draw_rounded_rect_aa(&tmp_surf, SOL_RECT(2, 2, SRC_SZ - 4, SRC_SZ - 4), 18, 4,
                             SOL_COLOR_WHITE);

    if (sol_image_encode_png(&tmp_surf, png_buf, sizeof(png_buf), &png_len) == SOL_OK) {
        if (sol_image_load_png_mem(out_img, png_buf, png_len, NULL) == SOL_OK) {
            return;
        }
    }

    /* Ultra-safe emergency fallback */
    out_img->pixels = NULL;
    out_img->owns_memory = false;
}

static void aim_randomize_velocity(aim_state_t *aim) {
    /* Random speed in [2..9] px/tick (~120..560 px/sec) in random direction */
    int32_t speed_fp8 = aim_rand_range(&aim->rng, 380, 1850);
    int32_t vx = aim_rand_range(&aim->rng, -1000, 1000);
    int32_t vy = aim_rand_range(&aim->rng, -1000, 1000);
    int64_t len2;

    if (vx == 0 && vy == 0) {
        vx = 700;
        vy = -500;
    }

    len2 = (int64_t)vx * (int64_t)vx + (int64_t)vy * (int64_t)vy;
    /* Normalize via integer sqrt approx */
    {
        uint32_t d = 1u;
        uint64_t bit = (uint64_t)1 << 30;
        uint64_t n = (uint64_t)len2;
        uint64_t res = 0u;
        while (bit > n) {
            bit >>= 2;
        }
        while (bit != 0u) {
            if (n >= res + bit) {
                n -= res + bit;
                res = (res >> 1) + bit;
            } else {
                res >>= 1;
            }
            bit >>= 2;
        }
        d = (res > 0u) ? (uint32_t)res : 1u;
        aim->vel_x_fp8 = (int32_t)(((int64_t)vx * (int64_t)speed_fp8) / (int64_t)d);
        aim->vel_y_fp8 = (int32_t)(((int64_t)vy * (int64_t)speed_fp8) / (int64_t)d);
    }
}

static void aim_init(aim_state_t *aim, const char *png_path, uint64_t now_ms) {
    sol_image_t img;
    bool loaded = false;
    size_t i;

    aim->rng = (uint32_t)(now_ms ^ 0xA5A55A5Aull);
    if (aim->rng == 0u) {
        aim->rng = 0x13572468u;
    }

    aim->pos_x_fp8 = 350 * 256;
    aim->pos_y_fp8 = 230 * 256;
    aim_randomize_velocity(aim);
    aim->next_change_ms = now_ms + 600u;
    aim->next_jump_ms = now_ms + 2400u;
    aim->hover_blend = 0u;
    aim->hits = 0u;
    aim->misses = 0u;

    for (i = 0u; i < AIM_MAX_WAVES; ++i) {
        aim->waves[i].active = false;
    }

    sol_surface_init(&aim->normal_surf, aim->normal_px, AIM_TARGET_SIZE, AIM_TARGET_SIZE,
                     AIM_TARGET_SIZE);
    sol_surface_init(&aim->green_surf, aim->green_px, AIM_TARGET_SIZE, AIM_TARGET_SIZE,
                     AIM_TARGET_SIZE);
    sol_surface_init(&aim->work_surf, aim->work_px, AIM_TARGET_SIZE, AIM_TARGET_SIZE,
                     AIM_TARGET_SIZE);

    if (png_path != NULL) {
        sol_status_t st = sol_image_load_file(&img, png_path, NULL);
        if (st == SOL_OK) {
            loaded = true;
            printf("[Aim] Loaded '%s' (%dx%d)\n", png_path, img.surface.width, img.surface.height);
        } else {
            fprintf(stderr, "[Aim] Failed to load '%s' (status=%d)\n", png_path, (int)st);
        }
    }
    if (!loaded && sol_image_load_file(&img, "target.png", NULL) == SOL_OK) {
        loaded = true;
        printf("[Aim] Loaded 'target.png' (%dx%d)\n", img.surface.width, img.surface.height);
    }
    if (!loaded) {
        aim_build_fallback_png(&img);
        loaded = (img.pixels != NULL);
    }

    if (loaded) {
        /* Force-scale any PNG dimensions to exact AIM_TARGET_SIZE x AIM_TARGET_SIZE */
        sol_clear(&aim->normal_surf, SOL_COLOR_TRANSPARENT);
        sol_blit_scaled(&aim->normal_surf, SOL_RECT(0, 0, AIM_TARGET_SIZE, AIM_TARGET_SIZE),
                        &img.surface, SOL_RECT(0, 0, img.surface.width, img.surface.height), true,
                        SOL_BLEND_NONE);
        sol_image_destroy(&img);
    } else {
        sol_clear(&aim->normal_surf, SOL_COLOR_RED);
    }

    /* Precompute green-hue shifted version (hue = 120 deg) preserving value, saturation & alpha */
    for (i = 0u; i < (size_t)(AIM_TARGET_SIZE * AIM_TARGET_SIZE); ++i) {
        uint32_t c = aim->normal_px[i];
        uint16_t h = 0u;
        uint8_t s = 0u;
        uint8_t v = 0u;
        uint8_t green_s;
        sol_color_to_hsv(c, &h, &s, &v);
        green_s = (s < 120u) ? 140u : s;
        aim->green_px[i] = sol_color_from_hsv(120u, green_s, v, SOL_COLOR_A(c));
    }

    aim->initialized = true;
}

static void aim_spawn_wave(aim_state_t *aim, int32_t x, int32_t y, bool is_hit, uint64_t now_ms) {
    size_t i;
    size_t oldest_idx = 0u;
    uint64_t oldest_ms = UINT64_MAX;

    for (i = 0u; i < AIM_MAX_WAVES; ++i) {
        if (!aim->waves[i].active) {
            aim->waves[i].active = true;
            aim->waves[i].is_hit = is_hit;
            aim->waves[i].x = x;
            aim->waves[i].y = y;
            aim->waves[i].start_ms = now_ms;
            return;
        }
        if (aim->waves[i].start_ms < oldest_ms) {
            oldest_ms = aim->waves[i].start_ms;
            oldest_idx = i;
        }
    }

    aim->waves[oldest_idx].active = true;
    aim->waves[oldest_idx].is_hit = is_hit;
    aim->waves[oldest_idx].x = x;
    aim->waves[oldest_idx].y = y;
    aim->waves[oldest_idx].start_ms = now_ms;
}

static void aim_update_and_click(aim_state_t *aim, sol_surface_t *surf, int32_t mx, int32_t my,
                                 bool clicked, uint64_t now_ms) {
    int32_t min_x = 12;
    int32_t min_y = 64;
    int32_t max_x = surf->width - AIM_TARGET_SIZE - 12;
    int32_t max_y = surf->height - AIM_TARGET_SIZE - 12;
    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }
    sol_rect_t target_rect;
    bool hovered;

    if (now_ms >= aim->next_change_ms) {
        aim_randomize_velocity(aim);
        aim->next_change_ms = now_ms + (uint64_t)aim_rand_range(&aim->rng, 250, 950);
    }

    if (now_ms >= aim->next_jump_ms) {
        aim->pos_x_fp8 = aim_rand_range(&aim->rng, min_x, max_x) * 256;
        aim->pos_y_fp8 = aim_rand_range(&aim->rng, min_y, max_y) * 256;
        aim_randomize_velocity(aim);
        aim->next_jump_ms = now_ms + (uint64_t)aim_rand_range(&aim->rng, 1600, 3200);
    }

    aim->pos_x_fp8 += aim->vel_x_fp8;
    aim->pos_y_fp8 += aim->vel_y_fp8;

    if (aim->pos_x_fp8 < min_x * 256) {
        aim->pos_x_fp8 = min_x * 256;
        aim->vel_x_fp8 = -aim->vel_x_fp8;
    } else if (aim->pos_x_fp8 > max_x * 256) {
        aim->pos_x_fp8 = max_x * 256;
        aim->vel_x_fp8 = -aim->vel_x_fp8;
    }

    if (aim->pos_y_fp8 < min_y * 256) {
        aim->pos_y_fp8 = min_y * 256;
        aim->vel_y_fp8 = -aim->vel_y_fp8;
    } else if (aim->pos_y_fp8 > max_y * 256) {
        aim->pos_y_fp8 = max_y * 256;
        aim->vel_y_fp8 = -aim->vel_y_fp8;
    }

    target_rect =
        SOL_RECT(aim->pos_x_fp8 >> 8, aim->pos_y_fp8 >> 8, AIM_TARGET_SIZE, AIM_TARGET_SIZE);
    hovered = sol_rect_contains_point(target_rect, mx, my);

    if (hovered) {
        aim->hover_blend = (aim->hover_blend + 48u > 255u) ? 255u : (aim->hover_blend + 48u);
    } else {
        aim->hover_blend = (aim->hover_blend < 32u) ? 0u : (aim->hover_blend - 32u);
    }

    if (clicked && my > 52) {
        if (hovered) {
            aim->hits++;
            aim_spawn_wave(aim, target_rect.x + AIM_TARGET_SIZE / 2,
                           target_rect.y + AIM_TARGET_SIZE / 2, true, now_ms);
            aim_randomize_velocity(aim);
            aim->next_change_ms = now_ms + (uint64_t)aim_rand_range(&aim->rng, 250, 800);
        } else {
            aim->misses++;
            aim_spawn_wave(aim, mx, my, false, now_ms);
        }
    }
}

static void render_scene_aim_test(sol_surface_t *surf, aim_state_t *aim, int32_t mx, int32_t my,
                                  uint64_t now_ms) {
    int32_t tx = aim->pos_x_fp8 >> 8;
    int32_t ty = aim->pos_y_fp8 >> 8;
    size_t i;

    /* Subtle dark aim-trainer grid arena */
    sol_fill_rect_gradient_v(surf, SOL_RECT(0, 53, surf->width, surf->height - 53),
                             SOL_RGB(12, 14, 24), SOL_RGB(20, 24, 38));
    {
        int32_t gx;
        int32_t gy;
        for (gx = 0; gx < surf->width; gx += 48) {
            sol_draw_vline(surf, gx, 53, surf->height - 53, SOL_ARGB(20, 255, 255, 255));
        }
        for (gy = 53; gy < surf->height; gy += 48) {
            sol_draw_hline(surf, 0, gy, surf->width, SOL_ARGB(20, 255, 255, 255));
        }
    }

    /* Render shockwaves behind & around the target */
    for (i = 0u; i < AIM_MAX_WAVES; ++i) {
        if (!aim->waves[i].active) {
            continue;
        }
        {
            uint64_t elapsed = now_ms - aim->waves[i].start_ms;
            uint64_t dur = aim->waves[i].is_hit ? 520u : 260u;
            if (elapsed >= dur) {
                aim->waves[i].active = false;
                continue;
            }

            {
                uint32_t t255 = (uint32_t)((elapsed * 255u) / dur);
                uint8_t alpha = (uint8_t)(((255u - t255) * (255u - t255)) >> 8);

                if (aim->waves[i].is_hit) {
                    int32_t r1 = (AIM_TARGET_SIZE / 2) + (int32_t)((t255 * 95u) / 255u);
                    int32_t r2 = (AIM_TARGET_SIZE / 2) + (int32_t)((t255 * 55u) / 255u);
                    sol_draw_circle_aa(surf, aim->waves[i].x, aim->waves[i].y, r1, 4,
                                       SOL_ARGB((uint8_t)((alpha * 180u) / 255u), 90, 255, 150));
                    sol_draw_circle_aa(surf, aim->waves[i].x, aim->waves[i].y, r2, 2,
                                       SOL_ARGB((uint8_t)((alpha * 130u) / 255u), 200, 255, 220));
                } else {
                    int32_t r = 6 + (int32_t)((t255 * 22u) / 255u);
                    sol_draw_circle_aa(surf, aim->waves[i].x, aim->waves[i].y, r, 2,
                                       SOL_ARGB((uint8_t)((alpha * 160u) / 255u), 255, 80, 90));
                }
            }
        }
    }

    /* Soft shadow behind the target + force-scaled PNG with smooth HSV green hue shift */
    sol_draw_box_shadow(surf, SOL_RECT(tx, ty, AIM_TARGET_SIZE, AIM_TARGET_SIZE), 12, 0, 8, 18, 0,
                        SOL_ARGB(140, 0, 0, 0));

    if (aim->hover_blend == 0u) {
        sol_blit(surf, tx, ty, &aim->normal_surf);
    } else if (aim->hover_blend == 255u) {
        sol_blit(surf, tx, ty, &aim->green_surf);
    } else {
        uint8_t t = (uint8_t)aim->hover_blend;
        for (i = 0u; i < (size_t)(AIM_TARGET_SIZE * AIM_TARGET_SIZE); ++i) {
            aim->work_px[i] = sol_color_lerp(aim->normal_px[i], aim->green_px[i], t);
        }
        sol_blit(surf, tx, ty, &aim->work_surf);
    }

    if (aim->hover_blend > 0u) {
        sol_draw_rounded_rect_aa(surf,
                                 SOL_RECT(tx - 2, ty - 2, AIM_TARGET_SIZE + 4, AIM_TARGET_SIZE + 4),
                                 10, 2, SOL_ARGB((uint8_t)aim->hover_blend, 100, 255, 140));
    }

    /* Precision aim crosshair on cursor */
    if (my > 52) {
        sol_color_t ch_col = (aim->hover_blend > 128u) ? SOL_RGB(100, 255, 140) : SOL_COLOR_WHITE;
        sol_draw_circle_aa(surf, mx, my, 10, 2, SOL_ARGB(200, 255, 255, 255));
        sol_draw_line(surf, mx - 16, my, mx - 5, my, ch_col);
        sol_draw_line(surf, mx + 5, my, mx + 16, my, ch_col);
        sol_draw_line(surf, mx, my - 16, mx, my - 5, ch_col);
        sol_draw_line(surf, mx, my + 5, mx, my + 16, ch_col);
    }
}

static void draw_topbar(sol_surface_t *surf, int32_t active_tab, int32_t mouse_x, int32_t mouse_y) {
    int32_t i;
    sol_color_t tab_colors[6] = {SOL_RGB(90, 140, 255),  SOL_RGB(255, 110, 120),
                                 SOL_RGB(70, 210, 160),  SOL_RGB(255, 185, 60),
                                 SOL_RGB(185, 110, 255), SOL_RGB(80, 235, 120)};

    sol_fill_rect(surf, SOL_RECT(0, 0, surf->width, 52), SOL_ARGB(235, 18, 20, 32));
    sol_draw_hline(surf, 0, 52, surf->width, SOL_ARGB(90, 255, 255, 255));

    for (i = 0; i < 6; ++i) {
        sol_rect_t tab = SOL_RECT(16 + i * 116, 10, 104, 32);
        bool hovered = sol_rect_contains_point(tab, mouse_x, mouse_y);
        bool active = (i == active_tab);
        sol_color_t bg =
            active ? tab_colors[i] : (hovered ? SOL_RGB(52, 58, 84) : SOL_RGB(34, 38, 56));

        sol_fill_rounded_rect_aa(surf, tab, 8, bg);
        if (active) {
            sol_draw_rounded_rect_aa(surf, tab, 8, 1, SOL_COLOR_WHITE);
        }

        if (i == 0) {
            sol_fill_circle_aa(surf, tab.x + 52, tab.y + 16, 7, SOL_COLOR_WHITE);
        } else if (i == 1) {
            sol_fill_circle_aa(surf, tab.x + 47, tab.y + 16, 6, SOL_ARGB(190, 255, 255, 255));
            sol_fill_circle_aa(surf, tab.x + 57, tab.y + 16, 6, SOL_ARGB(190, 255, 255, 255));
        } else if (i == 2) {
            sol_fill_rect_gradient_h(surf, SOL_RECT(tab.x + 34, tab.y + 10, 36, 12),
                                     SOL_COLOR_WHITE, SOL_RGB(40, 40, 80));
        } else if (i == 3) {
            sol_draw_rect(surf, SOL_RECT(tab.x + 40, tab.y + 9, 24, 14), SOL_COLOR_WHITE);
            sol_fill_rect(surf, SOL_RECT(tab.x + 45, tab.y + 13, 14, 6), SOL_COLOR_WHITE);
        } else if (i == 4) {
            sol_fill_rounded_rect_aa(surf, SOL_RECT(tab.x + 38, tab.y + 8, 28, 16), 4,
                                     SOL_ARGB(150, 255, 255, 255));
            sol_draw_rounded_rect_aa(surf, SOL_RECT(tab.x + 38, tab.y + 8, 28, 16), 4, 1,
                                     SOL_COLOR_WHITE);
        } else {
            /* Tab 6: Crosshair icon for Aim Test */
            sol_draw_circle_aa(surf, tab.x + 52, tab.y + 16, 7, 2, SOL_COLOR_WHITE);
            sol_draw_line(surf, tab.x + 42, tab.y + 16, tab.x + 62, tab.y + 16, SOL_COLOR_WHITE);
            sol_draw_line(surf, tab.x + 52, tab.y + 6, tab.x + 52, tab.y + 26, SOL_COLOR_WHITE);
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

    sol_fill_circle(surf, 460 - anim / 2, 145, 62, SOL_ARGB(170, 255, 70, 90));
    sol_fill_circle(surf, 520 + anim / 2, 145, 62, SOL_ARGB(170, 70, 230, 140));
    sol_fill_circle(surf, 490, 200 + anim / 3, 62, SOL_ARGB(170, 70, 140, 255));

    sol_fill_triangle(surf, 90, 390, 190, 265, 290, 390, SOL_ARGB(220, 255, 165, 40));
    sol_draw_triangle(surf, 90, 390, 190, 265, 290, 390, SOL_COLOR_WHITE);

    sol_fill_ellipse(surf, 490, 345, 115, 46, SOL_ARGB(180, 40, 190, 230));
    sol_draw_ellipse(surf, 490, 345, 115, 46, SOL_COLOR_CYAN);

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

    sol_fill_rect(surf, SOL_RECT(0, 53, surf->width, surf->height - 53), SOL_RGB(14, 16, 28));

    sol_fill_rect_gradient_linear(surf, SOL_RECT(24, 76, 350, 220), 24, 76, 374, 296, sunset, 4u,
                                  true);
    sol_draw_rect(surf, SOL_RECT(24, 76, 350, 220), SOL_COLOR_WHITE);

    sol_fill_rect_gradient_4(surf, SOL_RECT(400, 76, 350, 220), SOL_COLOR_RED, SOL_COLOR_GREEN,
                             SOL_COLOR_WHITE, SOL_COLOR_BLUE);
    sol_draw_rect(surf, SOL_RECT(400, 76, 350, 220), SOL_COLOR_WHITE);

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
        sol_fill_circle_aa(&panel, anim_x, 120, 65, SOL_ARGB(210, 255, 110, 90));
        sol_fill_rounded_rect_aa(&panel, SOL_RECT(140, 90, 180, 180), 20,
                                 SOL_ARGB(180, 70, 220, 160));
        sol_pop_clip(&panel, &stack);

        sol_draw_rect(&panel, SOL_RECT(0, 0, panel.width, panel.height), SOL_COLOR_CYAN);
    }

    if (sol_surface_subview(&panel, surf, SOL_RECT(430, 90, 310, 280)) == SOL_OK) {
        sol_clear(&panel, SOL_RGB(48, 30, 52));
        sol_fill_triangle_aa(&panel, anim_x, 40, 280, 240, 30, 220, SOL_RGB(255, 190, 60));
        sol_draw_rect(&panel, SOL_RECT(0, 0, panel.width, panel.height), SOL_COLOR_MAGENTA);
    }
}

static void render_scene_aa_glass(sol_surface_t *surf, int32_t mx, int32_t my, int32_t radius,
                                  uint64_t tick) {
    sol_rect_t glass = SOL_RECT(mx - 150, my - 100, 300, 200);
    int32_t spin = (int32_t)((tick / 8u) % 360u);

    sol_fill_rect_gradient_4(surf, SOL_RECT(0, 53, surf->width, surf->height - 53),
                             SOL_RGB(20, 30, 80), SOL_RGB(180, 45, 110), SOL_RGB(240, 160, 50),
                             SOL_RGB(30, 170, 160));

    sol_fill_circle_aa(surf, 200, 260, 95, SOL_RGB(255, 85, 110));
    sol_fill_triangle_aa(surf, 480, 380, 620, 140, 720, 380, SOL_RGB(90, 230, 170));
    sol_draw_line_thick_aa(surf, 60, 440, 740, 110, 12, SOL_CAP_ROUND,
                           SOL_ARGB(200, 255, 255, 255));
    sol_draw_circle_aa(surf, 200, 260, 115, 6, SOL_COLOR_WHITE);
    sol_fill_pie(surf, 620, 160, 48, spin, spin + 240, SOL_ARGB(220, 255, 230, 80));

    if (glass.y < 58) {
        glass.y = 58;
    }
    sol_draw_box_shadow(surf, glass, 18, 0, 12, 24, 2, SOL_ARGB(140, 0, 0, 0));
    sol_blur_rounded_rect(surf, glass, 18, (radius / 3) + 4);
    sol_fill_rounded_rect_aa(surf, glass, 18, SOL_ARGB(110, 25, 32, 52));
    sol_fill_rounded_rect_ex_aa(surf, SOL_RECT(glass.x, glass.y, glass.w, 36),
                                SOL_CORNERS(18, 18, 0, 0), SOL_ARGB(130, 255, 255, 255));
    sol_draw_rounded_rect_aa(surf, glass, 18, 2, SOL_ARGB(180, 255, 255, 255));
    sol_draw_line_dashed(surf, glass.x + 20, glass.y + 100, glass.x + glass.w - 20, glass.y + 100,
                         8, 5, SOL_ARGB(180, 255, 255, 255));
}

int main(int argc, char **argv) {
    sol_window_t *win = sol_window_create("SolsticeSFG Sandbox — Aim Test", 800, 520);
    const char *custom_png = (argc > 1) ? argv[1] : NULL;
    sol_event_t ev;
    bool running = true;
    int32_t active_tab = 5; /* Start right on Scene 6 (Ultimate Aim Test)! */
    int32_t mouse_x = 400;
    int32_t mouse_y = 280;
    int32_t radius = 48;
    uint64_t start_ms = sol_time_ms();
    uint64_t fps_timer = start_ms;
    uint32_t frames = 0;
    static aim_state_t s_aim;

    if (win == NULL) {
        fprintf(stderr, "Failed to open SolsticeSFG window\n");
        return 1;
    }

    aim_init(&s_aim, custom_png, start_ms);

    while (running) {
        bool clicked = false;

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
                mouse_x = ev.x;
                mouse_y = ev.y;
                if (ev.button == SOL_MOUSE_LEFT) {
                    if (ev.y >= 10 && ev.y <= 42) {
                        int32_t i;
                        for (i = 0; i < 6; ++i) {
                            if (sol_rect_contains_point(SOL_RECT(16 + i * 116, 10, 104, 32), ev.x,
                                                        ev.y)) {
                                active_tab = i;
                            }
                        }
                    } else {
                        clicked = true;
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
                } else if (ev.keycode >= '1' && ev.keycode <= '6') {
                    active_tab = (int32_t)(ev.keycode - '1');
                } else if (ev.keycode == 'R') {
                    s_aim.hits = 0u;
                    s_aim.misses = 0u;
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
                sol_surface_set_clip(surf, SOL_RECT(0, 53, surf->width, surf->height - 53));

                if (active_tab == 0) {
                    render_scene_shapes(surf, mouse_x, mouse_y, radius, tick);
                } else if (active_tab == 1) {
                    render_scene_blending(surf, mouse_x, mouse_y, radius);
                } else if (active_tab == 2) {
                    render_scene_gradients(surf, mouse_x, mouse_y, radius);
                } else if (active_tab == 3) {
                    render_scene_subviews(surf, tick);
                } else if (active_tab == 4) {
                    render_scene_aa_glass(surf, mouse_x, mouse_y, radius, tick);
                } else {
                    aim_update_and_click(&s_aim, surf, mouse_x, mouse_y, clicked, now);
                    render_scene_aim_test(surf, &s_aim, mouse_x, mouse_y, now);
                }

                sol_surface_reset_clip(surf);
                draw_topbar(surf, active_tab, mouse_x, mouse_y);
                sol_window_present(win);
            }

            frames++;
            if (now - fps_timer >= 500u) {
                char title[160];
                uint32_t fps = (uint32_t)((frames * 1000u) / (now - fps_timer));
                if (active_tab == 5) {
                    uint32_t total = s_aim.hits + s_aim.misses;
                    uint32_t acc = (total > 0u) ? ((s_aim.hits * 100u) / total) : 100u;
                    snprintf(title, sizeof(title),
                             "SolsticeSFG Aim Test | Hits: %u | Misses: %u | Acc: %u%% (R=reset) | "
                             "%u FPS",
                             (unsigned int)s_aim.hits, (unsigned int)s_aim.misses,
                             (unsigned int)acc, (unsigned int)fps);
                } else {
                    snprintf(title, sizeof(title),
                             "SolsticeSFG Sandbox | Scene %d (keys 1-6, ESC=quit) | %u FPS",
                             (int)(active_tab + 1), (unsigned int)fps);
                }
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
