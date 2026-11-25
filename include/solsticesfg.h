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

/* Core Pixel Operations */
SOL_API void sol_clear(sol_surface_t *surf, sol_color_t color);
SOL_API void sol_put_pixel(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color);
SOL_API sol_color_t sol_get_pixel(const sol_surface_t *surf, int32_t x, int32_t y);

#ifdef __cplusplus
}
#endif

#endif /* SOLSTICESFG_H */
