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

typedef enum sol_blend_mode {
    SOL_BLEND_NONE = 0, /* Direct overwrite (ignores dst, copies src as-is) */
    SOL_BLEND_SRC_OVER, /* Standard Porter-Duff alpha compositing */
    SOL_BLEND_SRC_IN,
    SOL_BLEND_SRC_OUT,
    SOL_BLEND_SRC_ATOP,
    SOL_BLEND_DST_OVER,
    SOL_BLEND_ADD,      /* Additive glow */
    SOL_BLEND_MULTIPLY, /* Darkening multiply blended by src alpha */
    SOL_BLEND_SCREEN,   /* Lightening screen blended by src alpha */
    SOL_BLEND_OVERLAY   /* Contrast overlay blended by src alpha */
} sol_blend_mode_t;

typedef struct sol_gradient_stop {
    uint8_t pos; /* 0..255 */
    sol_color_t color;
} sol_gradient_stop_t;

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

typedef struct sol_point {
    int32_t x;
    int32_t y;
} sol_point_t;

typedef struct sol_size {
    int32_t w;
    int32_t h;
} sol_size_t;

#define SOL_POINT(x, y) ((sol_point_t){(x), (y)})
#define SOL_SIZE(w, h) ((sol_size_t){(w), (h)})

#define SOL_CLIP_STACK_MAX 32
#define SOL_DIRTY_MAX_RECTS 32

typedef struct sol_clip_stack {
    sol_rect_t rects[SOL_CLIP_STACK_MAX];
    int32_t depth;
} sol_clip_stack_t;

typedef struct sol_dirty_tracker {
    sol_rect_t rects[SOL_DIRTY_MAX_RECTS];
    int32_t count;
    sol_rect_t bounds;
    bool full_redraw;
} sol_dirty_tracker_t;

typedef struct sol_corner_radii {
    int32_t tl;
    int32_t tr;
    int32_t br;
    int32_t bl;
} sol_corner_radii_t;

#define SOL_CORNERS(tl, tr, br, bl) ((sol_corner_radii_t){(tl), (tr), (br), (bl)})

typedef enum sol_line_cap { SOL_CAP_BUTT = 0, SOL_CAP_ROUND, SOL_CAP_SQUARE } sol_line_cap_t;

/* Core & Surface Management */
SOL_API const char *sol_version_string(void);

SOL_API sol_status_t sol_surface_init(sol_surface_t *surf, uint32_t *pixels, int32_t width,
                                      int32_t height, int32_t stride);
SOL_API void sol_surface_set_clip(sol_surface_t *surf, sol_rect_t clip);
SOL_API void sol_surface_reset_clip(sol_surface_t *surf);
SOL_API sol_status_t sol_surface_subview(sol_surface_t *dst, const sol_surface_t *src,
                                         sol_rect_t region);
SOL_API void sol_clip_intersect(sol_surface_t *surf, sol_rect_t rect);
SOL_API void sol_clip_stack_init(sol_clip_stack_t *stack);
SOL_API sol_status_t sol_push_clip(sol_surface_t *surf, sol_clip_stack_t *stack, sol_rect_t rect);
SOL_API sol_status_t sol_pop_clip(sol_surface_t *surf, sol_clip_stack_t *stack);

/* Dirty Rectangle Tracker */
SOL_API sol_status_t sol_dirty_init(sol_dirty_tracker_t *dt, int32_t width, int32_t height);
SOL_API void sol_dirty_reset(sol_dirty_tracker_t *dt);
SOL_API void sol_dirty_mark_all(sol_dirty_tracker_t *dt);
SOL_API void sol_dirty_add(sol_dirty_tracker_t *dt, sol_rect_t rect);
SOL_API bool sol_dirty_is_empty(const sol_dirty_tracker_t *dt);
SOL_API sol_rect_t sol_dirty_bounding_box(const sol_dirty_tracker_t *dt);

/* Geometry Helpers */
SOL_API sol_rect_t sol_rect_intersect(sol_rect_t a, sol_rect_t b);
SOL_API sol_rect_t sol_rect_intersect(sol_rect_t a, sol_rect_t b);
SOL_API sol_rect_t sol_rect_union(sol_rect_t a, sol_rect_t b);
SOL_API sol_rect_t sol_rect_inset(sol_rect_t rect, int32_t dx, int32_t dy);
SOL_API bool sol_rect_contains_point(sol_rect_t rect, int32_t x, int32_t y);
SOL_API bool sol_rect_equals(sol_rect_t a, sol_rect_t b);

/* Color & Blending */
SOL_API sol_color_t sol_color_with_alpha(sol_color_t color, uint8_t alpha);
SOL_API sol_color_t sol_color_lerp(sol_color_t a, sol_color_t b, uint8_t t);
SOL_API sol_color_t sol_color_lerp_linear(sol_color_t a, sol_color_t b, uint8_t t);
SOL_API sol_color_t sol_color_blend(sol_color_t dst, sol_color_t src);
SOL_API sol_color_t sol_color_blend_mode(sol_color_t dst, sol_color_t src, sol_blend_mode_t mode);

/* Core Pixel Operations */
SOL_API void sol_clear(sol_surface_t *surf, sol_color_t color);
SOL_API void sol_put_pixel(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color);
SOL_API void sol_blend_pixel(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color);
SOL_API sol_color_t sol_get_pixel(const sol_surface_t *surf, int32_t x, int32_t y);

/* Premultiplied Alpha */
SOL_API sol_color_t sol_color_premul(sol_color_t color);
SOL_API sol_color_t sol_color_unpremul(sol_color_t premul_color);
SOL_API sol_color_t sol_color_blend_premul(sol_color_t dst_pm, sol_color_t src_pm);

/* Lines */
SOL_API void sol_draw_hline(sol_surface_t *surf, int32_t x, int32_t y, int32_t len,
                            sol_color_t color);
SOL_API void sol_draw_vline(sol_surface_t *surf, int32_t x, int32_t y, int32_t len,
                            sol_color_t color);
SOL_API void sol_draw_line(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                           sol_color_t color);

/* Gamma & Color Spaces */
SOL_API uint16_t sol_srgb_to_linear(uint8_t srgb);
SOL_API uint8_t sol_linear_to_srgb(uint16_t linear);

SOL_API sol_color_t sol_color_from_hsv(uint16_t hue_deg, uint8_t sat, uint8_t val, uint8_t alpha);
SOL_API void sol_color_to_hsv(sol_color_t color, uint16_t *out_hue, uint8_t *out_sat,
                              uint8_t *out_val);
SOL_API sol_color_t sol_color_from_hsl(uint16_t hue_deg, uint8_t sat, uint8_t light, uint8_t alpha);
SOL_API void sol_color_to_hsl(sol_color_t color, uint16_t *out_hue, uint8_t *out_sat,
                              uint8_t *out_light);

SOL_API sol_status_t sol_color_from_hex(const char *hex, sol_color_t *out_color);
SOL_API sol_status_t sol_color_to_hex(sol_color_t color, char *out_buf, size_t buf_size,
                                      bool include_alpha);

SOL_API uint8_t sol_color_luminance(sol_color_t color);
SOL_API uint32_t sol_color_contrast_x100(sol_color_t a, sol_color_t b);

/* Advanced Pixel & Gradient Blending */
SOL_API void sol_blend_pixel_mode(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color,
                                  sol_blend_mode_t mode);
SOL_API void sol_fill_rect_blend(sol_surface_t *surf, sol_rect_t rect, sol_color_t color,
                                 sol_blend_mode_t mode);

SOL_API sol_color_t sol_gradient_eval(const sol_gradient_stop_t *stops, size_t count, uint8_t t,
                                      bool linear_gamma);
SOL_API void sol_fill_rect_gradient_linear(sol_surface_t *surf, sol_rect_t rect, int32_t x0,
                                           int32_t y0, int32_t x1, int32_t y1,
                                           const sol_gradient_stop_t *stops, size_t stop_count,
                                           bool linear_gamma);
SOL_API void sol_fill_rect_gradient_radial(sol_surface_t *surf, sol_rect_t rect, int32_t cx,
                                           int32_t cy, int32_t radius,
                                           const sol_gradient_stop_t *stops, size_t stop_count,
                                           bool linear_gamma);
SOL_API void sol_fill_rect_gradient_4(sol_surface_t *surf, sol_rect_t rect, sol_color_t tl,
                                      sol_color_t tr, sol_color_t br, sol_color_t bl);

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

/* Extended Hard-Edged Primitives */
SOL_API void sol_fill_rounded_rect_ex(sol_surface_t *surf, sol_rect_t rect,
                                      sol_corner_radii_t radii, sol_color_t color);
SOL_API void sol_draw_line_dashed(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1,
                                  int32_t y1, int32_t dash_len, int32_t gap_len, sol_color_t color);
SOL_API void sol_draw_arc(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                          int32_t start_deg, int32_t end_deg, sol_color_t color);
SOL_API void sol_fill_pie(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                          int32_t start_deg, int32_t end_deg, sol_color_t color);
SOL_API void sol_fill_ring(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t inner_radius,
                           int32_t outer_radius, sol_color_t color);

/* Anti-Aliased Primitives (sol_aa.c) */
SOL_API void sol_draw_line_aa(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                              sol_color_t color);
SOL_API void sol_draw_line_thick_aa(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1,
                                    int32_t y1, int32_t thickness, sol_line_cap_t cap,
                                    sol_color_t color);
SOL_API void sol_fill_circle_aa(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                                sol_color_t color);
SOL_API void sol_draw_circle_aa(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                                int32_t thickness, sol_color_t color);
SOL_API void sol_fill_ellipse_aa(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t rx,
                                 int32_t ry, sol_color_t color);
SOL_API void sol_fill_rounded_rect_aa(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                                      sol_color_t color);
SOL_API void sol_fill_rounded_rect_ex_aa(sol_surface_t *surf, sol_rect_t rect,
                                         sol_corner_radii_t radii, sol_color_t color);
SOL_API void sol_draw_rounded_rect_aa(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                                      int32_t thickness, sol_color_t color);
SOL_API void sol_fill_triangle_aa(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1,
                                  int32_t y1, int32_t x2, int32_t y2, sol_color_t color);

/* Visual Effects (sol_fx.c) */
SOL_API void sol_blur_rect(sol_surface_t *surf, sol_rect_t rect, int32_t radius);
SOL_API void sol_blur_rounded_rect(sol_surface_t *surf, sol_rect_t rect, int32_t corner_radius,
                                   int32_t blur_radius);
SOL_API void sol_draw_box_shadow(sol_surface_t *surf, sol_rect_t rect, int32_t corner_radius,
                                 int32_t offset_x, int32_t offset_y, int32_t blur_radius,
                                 int32_t spread, sol_color_t color);
SOL_API void sol_apply_noise(sol_surface_t *surf, sol_rect_t rect, uint8_t amount, uint32_t seed);

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
