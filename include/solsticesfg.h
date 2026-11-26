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

#ifndef SOL_FREESTANDING
    #if !defined(SOL_MALLOC) || !defined(SOL_REALLOC) || !defined(SOL_FREE)
        #include <stdlib.h>
    #endif
    #if !defined(SOL_MEMCPY) || !defined(SOL_MEMSET) || !defined(SOL_MEMMOVE) ||                   \
        !defined(SOL_MEMCMP)
        #include <string.h>
    #endif
#endif

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

/* ============================================================================
 * Phase 1.2: Freestanding Memory & Arena Allocator
 * ============================================================================ */

/* Pure C99 zero-libc memory fallbacks (used automatically under SOL_FREESTANDING) */
SOL_API void *sol__memcpy(void *dst, const void *src, size_t n);
SOL_API void *sol__memset(void *dst, int val, size_t n);
SOL_API void *sol__memmove(void *dst, const void *src, size_t n);
SOL_API int sol__memcmp(const void *a, const void *b, size_t n);

#ifdef SOL_FREESTANDING
    #ifndef SOL_MALLOC
        #define SOL_MALLOC(sz) ((void)(sz), (void *)0)
    #endif
    #ifndef SOL_REALLOC
        #define SOL_REALLOC(ptr, sz) ((void)(ptr), (void)(sz), (void *)0)
    #endif
    #ifndef SOL_FREE
        #define SOL_FREE(ptr) ((void)(ptr))
    #endif
    #ifndef SOL_MEMCPY
        #define SOL_MEMCPY(dst, src, n) sol__memcpy((dst), (src), (n))
    #endif
    #ifndef SOL_MEMSET
        #define SOL_MEMSET(dst, val, n) sol__memset((dst), (val), (n))
    #endif
    #ifndef SOL_MEMMOVE
        #define SOL_MEMMOVE(dst, src, n) sol__memmove((dst), (src), (n))
    #endif
    #ifndef SOL_MEMCMP
        #define SOL_MEMCMP(a, b, n) sol__memcmp((a), (b), (n))
    #endif
#else
    #ifndef SOL_MALLOC
        #define SOL_MALLOC(sz) malloc(sz)
    #endif
    #ifndef SOL_REALLOC
        #define SOL_REALLOC(ptr, sz) realloc((ptr), (sz))
    #endif
    #ifndef SOL_FREE
        #define SOL_FREE(ptr) free(ptr)
    #endif
    #ifndef SOL_MEMCPY
        #define SOL_MEMCPY(dst, src, n) memcpy((dst), (src), (n))
    #endif
    #ifndef SOL_MEMSET
        #define SOL_MEMSET(dst, val, n) memset((dst), (val), (n))
    #endif
    #ifndef SOL_MEMMOVE
        #define SOL_MEMMOVE(dst, src, n) memmove((dst), (src), (n))
    #endif
    #ifndef SOL_MEMCMP
        #define SOL_MEMCMP(a, b, n) memcmp((a), (b), (n))
    #endif
#endif

/* Caller-backed Arena / Bump Allocator (zero fragmentation, instant reset) */
typedef struct sol_arena {
    uint8_t *base;
    size_t capacity;
    size_t offset;
} sol_arena_t;

SOL_API sol_status_t sol_arena_init(sol_arena_t *arena, void *buffer, size_t capacity);
SOL_API void *sol_arena_alloc(sol_arena_t *arena, size_t size);
SOL_API void *sol_arena_alloc_aligned(sol_arena_t *arena, size_t size, size_t align);
SOL_API void *sol_arena_calloc(sol_arena_t *arena, size_t count, size_t size);
SOL_API void sol_arena_reset(sol_arena_t *arena);
SOL_API size_t sol_arena_remaining(const sol_arena_t *arena);
SOL_API size_t sol_arena_mark(const sol_arena_t *arena);
SOL_API void sol_arena_rewind(sol_arena_t *arena, size_t mark);

/* ============================================================================
 * Phase 1.3: Multi-Format Framebuffer Conversion
 * ============================================================================ */

typedef enum sol_pixel_format {
    SOL_FMT_ARGB8888 = 0, /* Canonical internal format: 0xAARRGGBB */
    SOL_FMT_XRGB8888,     /* Linux DRM/KMS, X11, Windows DIB */
    SOL_FMT_RGBA8888,     /* OpenGL / Wayland / macOS */
    SOL_FMT_BGRA8888,     /* Little-endian byte order on DRM/Wayland/Win32 */
    SOL_FMT_ABGR8888,     /* 0xAABBGGRR */
    SOL_FMT_RGB888,       /* 24-bit packed: R, G, B */
    SOL_FMT_BGR888,       /* 24-bit packed: B, G, R */
    SOL_FMT_RGB565,       /* 16-bit embedded / VESA BIOS */
    SOL_FMT_BGR565,       /* 16-bit BGR565 */
    SOL_FMT_A8,           /* 8-bit alpha mask (extracts A) */
    SOL_FMT_GRAY8         /* 8-bit grayscale luminance (BT.601) */
} sol_pixel_format_t;

SOL_API int32_t sol_format_bpp(sol_pixel_format_t fmt);
SOL_API sol_status_t sol_convert_row(void *dst, sol_pixel_format_t dst_fmt,
                                     const uint32_t *src_argb, int32_t width);
SOL_API sol_status_t sol_convert_row_dithered(void *dst, sol_pixel_format_t dst_fmt,
                                              const uint32_t *src_argb, int32_t width, int32_t y);
SOL_API sol_status_t sol_present_to_buffer(void *dst_pixels, int32_t dst_stride_bytes,
                                           sol_pixel_format_t dst_fmt, const sol_surface_t *src,
                                           bool dither);

#ifdef __cplusplus
}
#endif

#endif /* SOLSTICESFG_H */
