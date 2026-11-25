/*
 * SolsticeSFG - Solstice Single-File Graphics Library
 * Copyright (c) 2026 Equinox-Collective (ewasion137)
 * SPDX-License-Identifier: Zlib OR MIT
 */

#ifndef SOLSTICESFG_H
#define SOLSTICESFG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOL_VERSION_MAJOR 0
#define SOL_VERSION_MINOR 1
#define SOL_VERSION_PATCH 0

/* Symbol visibility for shared library builds */
#ifndef SOL_API
    #ifdef SOL_BUILD_SHARED
        #if defined(_WIN32) || defined(__CYGWIN__)
            #define SOL_API __declspec(dllexport)
        #elif defined(__GNUC__) && __GNUC__ >= 4
            #define SOL_API __attribute__((visibility("default")))
        #else
            #define SOL_API extern
        #endif
    #else
        #define SOL_API extern
    #endif
#endif

/* ARGB8888 Color Helpers (0xAARRGGBB) */
typedef uint32_t sol_color_t;

#define SOL_ARGB(a, r, g, b)                                                                       \
    ((((sol_color_t)(uint8_t)(a)) << 24) | (((sol_color_t)(uint8_t)(r)) << 16) |                   \
     (((sol_color_t)(uint8_t)(g)) << 8) | ((sol_color_t)(uint8_t)(b)))

#define SOL_RGB(r, g, b) SOL_ARGB(0xFFu, (r), (g), (b))

#define SOL_COLOR_A(c) ((uint8_t)(((c) >> 24) & 0xFFu))
#define SOL_COLOR_R(c) ((uint8_t)(((c) >> 16) & 0xFFu))
#define SOL_COLOR_G(c) ((uint8_t)(((c) >> 8) & 0xFFu))
#define SOL_COLOR_B(c) ((uint8_t)((c) & 0xFFu))

/* Built-in Color Constants */
#define SOL_COLOR_TRANSPARENT ((sol_color_t)0x00000000u)
#define SOL_COLOR_BLACK ((sol_color_t)0xFF000000u)
#define SOL_COLOR_WHITE ((sol_color_t)0xFFFFFFFFu)
#define SOL_COLOR_RED ((sol_color_t)0xFFFF0000u)
#define SOL_COLOR_GREEN ((sol_color_t)0xFF00FF00u)
#define SOL_COLOR_BLUE ((sol_color_t)0xFF0000FFu)
#define SOL_COLOR_YELLOW ((sol_color_t)0xFFFFFF00u)
#define SOL_COLOR_CYAN ((sol_color_t)0xFF00FFFFu)
#define SOL_COLOR_MAGENTA ((sol_color_t)0xFFFF00FFu)
#define SOL_COLOR_ORANGE ((sol_color_t)0xFFFF8800u)
#define SOL_COLOR_GRAY ((sol_color_t)0xFF808080u)
#define SOL_COLOR_DARK_GRAY ((sol_color_t)0xFF303030u)
#define SOL_COLOR_LIGHT_GRAY ((sol_color_t)0xFFC0C0C0u)

typedef enum sol_status {
    SOL_OK = 0,
    SOL_ERR_INVALID_ARG = -1,
    SOL_ERR_OUT_OF_BOUNDS = -2
} sol_status_t;

typedef struct sol_rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} sol_rect_t;

#define SOL_RECT(x, y, w, h) ((sol_rect_t){(x), (y), (w), (h)})
#define SOL_SQUARE(x, y, s) ((sol_rect_t){(x), (y), (s), (s)})

typedef struct sol_surface {
    uint32_t *pixels;
    int32_t width;
    int32_t height;
    int32_t stride; /* Row stride in pixels (>= width) */
    sol_rect_t clip;
} sol_surface_t;

/* Core & Surface Management */
SOL_API const char *sol_version_string(void);

SOL_API sol_status_t sol_surface_init(sol_surface_t *surf, uint32_t *pixels, int32_t width,
                                      int32_t height, int32_t stride);
SOL_API void sol_surface_set_clip(sol_surface_t *surf, sol_rect_t clip);
SOL_API void sol_surface_reset_clip(sol_surface_t *surf);

/* Geometry Helpers */
SOL_API sol_rect_t sol_rect_intersect(sol_rect_t a, sol_rect_t b);

/* Color & Blending */
SOL_API sol_color_t sol_color_with_alpha(sol_color_t color, uint8_t alpha);
SOL_API sol_color_t sol_color_lerp(sol_color_t a, sol_color_t b, uint8_t t);
SOL_API sol_color_t sol_color_blend(sol_color_t dst, sol_color_t src);

/* Core Pixel Operations */
SOL_API void sol_clear(sol_surface_t *surf, sol_color_t color);
SOL_API void sol_put_pixel(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color);
SOL_API void sol_blend_pixel(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color);
SOL_API sol_color_t sol_get_pixel(const sol_surface_t *surf, int32_t x, int32_t y);

/* Lines */
SOL_API void sol_draw_hline(sol_surface_t *surf, int32_t x, int32_t y, int32_t len,
                            sol_color_t color);
SOL_API void sol_draw_vline(sol_surface_t *surf, int32_t x, int32_t y, int32_t len,
                            sol_color_t color);
SOL_API void sol_draw_line(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                           sol_color_t color);

/* Rectangles, Squares & Gradients */
SOL_API void sol_fill_rect(sol_surface_t *surf, sol_rect_t rect, sol_color_t color);
SOL_API void sol_draw_rect(sol_surface_t *surf, sol_rect_t rect, sol_color_t color);
SOL_API void sol_draw_rect_thick(sol_surface_t *surf, sol_rect_t rect, int32_t thickness,
                                 sol_color_t color);
SOL_API void sol_fill_rect_gradient_v(sol_surface_t *surf, sol_rect_t rect, sol_color_t top,
                                      sol_color_t bottom);
SOL_API void sol_fill_rect_gradient_h(sol_surface_t *surf, sol_rect_t rect, sol_color_t left,
                                      sol_color_t right);

/* Rounded Rectangles */
SOL_API void sol_fill_rounded_rect(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                                   sol_color_t color);
SOL_API void sol_draw_rounded_rect(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                                   sol_color_t color);

/* Circles & Ellipses */
SOL_API void sol_fill_circle(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                             sol_color_t color);
SOL_API void sol_draw_circle(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                             sol_color_t color);
SOL_API void sol_fill_ellipse(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t rx, int32_t ry,
                              sol_color_t color);
SOL_API void sol_draw_ellipse(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t rx, int32_t ry,
                              sol_color_t color);

/* Triangles */
SOL_API void sol_draw_triangle(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                               int32_t x2, int32_t y2, sol_color_t color);
SOL_API void sol_fill_triangle(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                               int32_t x2, int32_t y2, sol_color_t color);

#ifdef __cplusplus
}
#endif

#endif /* SOLSTICESFG_H */
