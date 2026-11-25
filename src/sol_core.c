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
