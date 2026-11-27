#include "sol_internal.h"

const char *sol_version_string(void) {
    return "0.1.0";
}

sol_rect_t sol_rect_intersect(sol_rect_t a, sol_rect_t b) {
    sol_rect_t res = {0, 0, 0, 0};

    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) {
        return res;
    }

    int64_t a_x2 = (int64_t)a.x + (int64_t)a.w;
    int64_t a_y2 = (int64_t)a.y + (int64_t)a.h;
    int64_t b_x2 = (int64_t)b.x + (int64_t)b.w;
    int64_t b_y2 = (int64_t)b.y + (int64_t)b.h;

    int64_t x1 = SOL_MAX((int64_t)a.x, (int64_t)b.x);
    int64_t y1 = SOL_MAX((int64_t)a.y, (int64_t)b.y);
    int64_t x2 = SOL_MIN(a_x2, b_x2);
    int64_t y2 = SOL_MIN(a_y2, b_y2);

    if (x2 > x1 && y2 > y1) {
        res.x = (int32_t)x1;
        res.y = (int32_t)y1;
        res.w = (int32_t)(x2 - x1);
        res.h = (int32_t)(y2 - y1);
    }

    return res;
}

sol_status_t sol_surface_init(sol_surface_t *surf, uint32_t *pixels, int32_t width, int32_t height,
                              int32_t stride) {
    if (surf == NULL || pixels == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (width <= 0 || height <= 0 || stride < width) {
        return SOL_ERR_INVALID_ARG;
    }

    surf->pixels = pixels;
    surf->width = width;
    surf->height = height;
    surf->stride = stride;
    surf->clip.x = 0;
    surf->clip.y = 0;
    surf->clip.w = width;
    surf->clip.h = height;

    return SOL_OK;
}

void sol_surface_set_clip(sol_surface_t *surf, sol_rect_t clip) {
    sol_rect_t bounds;

    if (surf == NULL) {
        return;
    }

    bounds.x = 0;
    bounds.y = 0;
    bounds.w = surf->width;
    bounds.h = surf->height;
    surf->clip = sol_rect_intersect(bounds, clip);
}

void sol_surface_reset_clip(sol_surface_t *surf) {
    if (surf == NULL) {
        return;
    }

    surf->clip.x = 0;
    surf->clip.y = 0;
    surf->clip.w = surf->width;
    surf->clip.h = surf->height;
}

void sol_clear(sol_surface_t *surf, sol_color_t color) {
    int32_t row;
    int32_t col;

    if (surf == NULL || surf->pixels == NULL) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }

    for (row = 0; row < surf->clip.h; ++row) {
        size_t row_offset =
            (size_t)(surf->clip.y + row) * (size_t)surf->stride + (size_t)surf->clip.x;
        uint32_t *dst = surf->pixels + row_offset;

        for (col = 0; col < surf->clip.w; ++col) {
            dst[col] = color;
        }
    }
}

void sol_put_pixel(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color) {
    if (surf == NULL || surf->pixels == NULL) {
        return;
    }
    if (x < surf->clip.x || y < surf->clip.y || x >= surf->clip.x + surf->clip.w ||
        y >= surf->clip.y + surf->clip.h) {
        return;
    }

    surf->pixels[(size_t)y * (size_t)surf->stride + (size_t)x] = color;
}

sol_color_t sol_get_pixel(const sol_surface_t *surf, int32_t x, int32_t y) {
    if (surf == NULL || surf->pixels == NULL) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= surf->width || y >= surf->height) {
        return 0;
    }

    return surf->pixels[(size_t)y * (size_t)surf->stride + (size_t)x];
}

sol_rect_t sol_rect_union(sol_rect_t a, sol_rect_t b) {
    sol_rect_t res = {0, 0, 0, 0};
    bool a_empty = (a.w <= 0 || a.h <= 0);
    bool b_empty = (b.w <= 0 || b.h <= 0);

    if (a_empty && b_empty) {
        return res;
    }
    if (a_empty) {
        return b;
    }
    if (b_empty) {
        return a;
    }

    int64_t x1 = SOL_MIN((int64_t)a.x, (int64_t)b.x);
    int64_t y1 = SOL_MIN((int64_t)a.y, (int64_t)b.y);
    int64_t x2 = SOL_MAX((int64_t)a.x + (int64_t)a.w, (int64_t)b.x + (int64_t)b.w);
    int64_t y2 = SOL_MAX((int64_t)a.y + (int64_t)a.h, (int64_t)b.y + (int64_t)b.h);

    res.x = (int32_t)x1;
    res.y = (int32_t)y1;
    res.w = (int32_t)SOL_MIN(x2 - x1, (int64_t)INT32_MAX);
    res.h = (int32_t)SOL_MIN(y2 - y1, (int64_t)INT32_MAX);
    return res;
}

sol_rect_t sol_rect_inset(sol_rect_t rect, int32_t dx, int32_t dy) {
    sol_rect_t res = {0, 0, 0, 0};
    int64_t nx;
    int64_t ny;
    int64_t nw;
    int64_t nh;

    if (rect.w <= 0 || rect.h <= 0) {
        return res;
    }

    nx = (int64_t)rect.x + (int64_t)dx;
    ny = (int64_t)rect.y + (int64_t)dy;
    nw = (int64_t)rect.w - (int64_t)dx * 2;
    nh = (int64_t)rect.h - (int64_t)dy * 2;

    if (nw <= 0 || nh <= 0) {
        return res;
    }

    res.x = (int32_t)nx;
    res.y = (int32_t)ny;
    res.w = (int32_t)SOL_MIN(nw, (int64_t)INT32_MAX);
    res.h = (int32_t)SOL_MIN(nh, (int64_t)INT32_MAX);
    return res;
}

bool sol_rect_contains_point(sol_rect_t rect, int32_t x, int32_t y) {
    if (rect.w <= 0 || rect.h <= 0) {
        return false;
    }
    return ((int64_t)x >= (int64_t)rect.x) && ((int64_t)y >= (int64_t)rect.y) &&
           ((int64_t)x < (int64_t)rect.x + (int64_t)rect.w) &&
           ((int64_t)y < (int64_t)rect.y + (int64_t)rect.h);
}

bool sol_rect_equals(sol_rect_t a, sol_rect_t b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

sol_status_t sol_surface_subview(sol_surface_t *dst, const sol_surface_t *src, sol_rect_t region) {
    sol_rect_t bounds;
    sol_rect_t inter;

    if (dst == NULL || src == NULL || src->pixels == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (src->width <= 0 || src->height <= 0 || src->stride < src->width) {
        return SOL_ERR_INVALID_ARG;
    }

    bounds.x = 0;
    bounds.y = 0;
    bounds.w = src->width;
    bounds.h = src->height;
    inter = sol_rect_intersect(bounds, region);

    if (inter.w <= 0 || inter.h <= 0) {
        dst->pixels = NULL;
        dst->width = 0;
        dst->height = 0;
        dst->stride = 0;
        dst->clip = SOL_RECT(0, 0, 0, 0);
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    dst->pixels = src->pixels + (size_t)inter.y * (size_t)src->stride + (size_t)inter.x;
    dst->width = inter.w;
    dst->height = inter.h;
    dst->stride = src->stride;
    dst->clip = SOL_RECT(0, 0, inter.w, inter.h);
    return SOL_OK;
}

void sol_clip_intersect(sol_surface_t *surf, sol_rect_t rect) {
    if (surf == NULL) {
        return;
    }
    surf->clip = sol_rect_intersect(surf->clip, rect);
}

void sol_clip_stack_init(sol_clip_stack_t *stack) {
    if (stack == NULL) {
        return;
    }
    stack->depth = 0;
}

sol_status_t sol_push_clip(sol_surface_t *surf, sol_clip_stack_t *stack, sol_rect_t rect) {
    if (surf == NULL || stack == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (stack->depth < 0 || stack->depth >= SOL_CLIP_STACK_MAX) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    stack->rects[stack->depth++] = surf->clip;
    surf->clip = sol_rect_intersect(surf->clip, rect);
    return SOL_OK;
}

sol_status_t sol_pop_clip(sol_surface_t *surf, sol_clip_stack_t *stack) {
    if (surf == NULL || stack == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (stack->depth <= 0 || stack->depth > SOL_CLIP_STACK_MAX) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    surf->clip = stack->rects[--stack->depth];
    return SOL_OK;
}

SOL_PRIVATE bool sol__rect_contains_rect(sol_rect_t outer, sol_rect_t inner) {
    int64_t ox2 = (int64_t)outer.x + (int64_t)outer.w;
    int64_t oy2 = (int64_t)outer.y + (int64_t)outer.h;
    int64_t ix2 = (int64_t)inner.x + (int64_t)inner.w;
    int64_t iy2 = (int64_t)inner.y + (int64_t)inner.h;

    return (int64_t)inner.x >= (int64_t)outer.x && (int64_t)inner.y >= (int64_t)outer.y &&
           ix2 <= ox2 && iy2 <= oy2;
}

SOL_PRIVATE bool sol__rects_overlap_or_touch(sol_rect_t a, sol_rect_t b) {
    int64_t ax2 = (int64_t)a.x + (int64_t)a.w;
    int64_t ay2 = (int64_t)a.y + (int64_t)a.h;
    int64_t bx2 = (int64_t)b.x + (int64_t)b.w;
    int64_t by2 = (int64_t)b.y + (int64_t)b.h;

    return (int64_t)a.x <= bx2 && ax2 >= (int64_t)b.x && (int64_t)a.y <= by2 && ay2 >= (int64_t)b.y;
}

SOL_PRIVATE int64_t sol__rect_area(sol_rect_t r) {
    if (r.w <= 0 || r.h <= 0) {
        return 0;
    }
    return (int64_t)r.w * (int64_t)r.h;
}

sol_status_t sol_dirty_init(sol_dirty_tracker_t *dt, int32_t width, int32_t height) {
    if (dt == NULL || width <= 0 || height <= 0) {
        return SOL_ERR_INVALID_ARG;
    }

    dt->bounds = SOL_RECT(0, 0, width, height);
    dt->count = 0;
    dt->full_redraw = false;
    return SOL_OK;
}

void sol_dirty_reset(sol_dirty_tracker_t *dt) {
    if (dt == NULL) {
        return;
    }
    dt->count = 0;
    dt->full_redraw = false;
}

void sol_dirty_mark_all(sol_dirty_tracker_t *dt) {
    if (dt == NULL) {
        return;
    }
    dt->full_redraw = true;
    dt->count = 1;
    dt->rects[0] = dt->bounds;
}

void sol_dirty_add(sol_dirty_tracker_t *dt, sol_rect_t rect) {
    sol_rect_t r;
    int32_t i;

    if (dt == NULL || dt->full_redraw) {
        return;
    }

    r = sol_rect_intersect(dt->bounds, rect);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    /* Merge overlapping or contained rectangles iteratively */
    for (i = 0; i < dt->count; /* incremented conditionally */) {
        if (sol__rect_contains_rect(dt->rects[i], r)) {
            return;
        }
        if (sol__rect_contains_rect(r, dt->rects[i]) ||
            sol__rects_overlap_or_touch(r, dt->rects[i])) {
            r = sol_rect_union(r, dt->rects[i]);
            dt->rects[i] = dt->rects[--dt->count];
            i = 0; /* Re-check merged rect against earlier entries */
            continue;
        }
        ++i;
    }

    if (dt->count < SOL_DIRTY_MAX_RECTS) {
        dt->rects[dt->count++] = r;
        return;
    }

    /* Capacity full: merge into the rect that increases total area the least */
    {
        int32_t best_idx = 0;
        int64_t best_cost = INT64_MAX;

        for (i = 0; i < dt->count; ++i) {
            sol_rect_t u = sol_rect_union(dt->rects[i], r);
            int64_t cost = sol__rect_area(u) - sol__rect_area(dt->rects[i]);
            if (cost < best_cost) {
                best_cost = cost;
                best_idx = i;
            }
        }
        dt->rects[best_idx] = sol_rect_union(dt->rects[best_idx], r);
    }
}

bool sol_dirty_is_empty(const sol_dirty_tracker_t *dt) {
    if (dt == NULL) {
        return true;
    }
    return !dt->full_redraw && dt->count == 0;
}

sol_rect_t sol_dirty_bounding_box(const sol_dirty_tracker_t *dt) {
    sol_rect_t bbox = {0, 0, 0, 0};
    int32_t i;

    if (dt == NULL || sol_dirty_is_empty(dt)) {
        return bbox;
    }
    if (dt->full_redraw) {
        return dt->bounds;
    }

    bbox = dt->rects[0];
    for (i = 1; i < dt->count; ++i) {
        bbox = sol_rect_union(bbox, dt->rects[i]);
    }
    return bbox;
}

/* 0..90 deg sin LUT scaled by 65536 (pure C99 freestanding trig, zero libm) */
static const uint16_t k_sin90_fp16[91] = {
    0,     1144,  2287,  3430,  4572,  5712,  6850,  7987,  9121,  10252, 11380, 12505, 13626,
    14742, 15855, 16962, 18064, 19161, 20252, 21336, 22415, 23486, 24550, 25607, 26656, 27697,
    28729, 29753, 30767, 31772, 32768, 33754, 34729, 35693, 36647, 37590, 38521, 39441, 40348,
    41243, 42126, 42995, 43852, 44695, 45525, 46341, 47143, 47930, 48703, 49461, 50203, 50931,
    51643, 52339, 53020, 53684, 54332, 54963, 55578, 56175, 56756, 57319, 57865, 58393, 58903,
    59396, 59870, 60326, 60764, 61183, 61584, 61966, 62328, 62672, 62997, 63303, 63589, 63856,
    64104, 64332, 64540, 64729, 64898, 65048, 65177, 65287, 65376, 65446, 65496, 65526, 65535};

int32_t sol__sin_deg_fp16(int32_t deg) {
    int32_t d = deg % 360;
    if (d < 0) {
        d += 360;
    }
    if (d == 90) {
        return SOL_FX16_ONE;
    }
    if (d == 270) {
        return -SOL_FX16_ONE;
    }
    if (d <= 90) {
        return (int32_t)k_sin90_fp16[d];
    }
    if (d <= 180) {
        return (int32_t)k_sin90_fp16[180 - d];
    }
    if (d <= 270) {
        return -(int32_t)k_sin90_fp16[d - 180];
    }
    return -(int32_t)k_sin90_fp16[360 - d];
}

int32_t sol__cos_deg_fp16(int32_t deg) {
    return sol__sin_deg_fp16(deg + 90);
}
