/*
 * SolsticeSFG - Surface & Rasterizer Fuzz Target
 * SPDX-License-Identifier: Zlib OR MIT
 *
 * Compatible with libFuzzer (LLVMFuzzerTestOneInput) and standalone smoke mode
 * (-DSOL_FUZZ_STANDALONE).
 */

#include <stddef.h>
#include <stdint.h>

#include "solsticesfg.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int32_t read_i32(const uint8_t *data, size_t size, size_t *offset) {
    uint32_t val = 0;
    size_t i;
    for (i = 0; i < 4; ++i) {
        uint8_t b = (*offset < size) ? data[(*offset)++] : 0;
        val |= ((uint32_t)b) << (i * 8u);
    }
    return (int32_t)val;
}

static sol_color_t read_color(const uint8_t *data, size_t size, size_t *offset) {
    return (sol_color_t)read_i32(data, size, offset);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    enum { MAX_W = 64, MAX_H = 64, BUF_DIM = 72 };
    uint32_t pixels[BUF_DIM * BUF_DIM];
    sol_surface_t surf;
    size_t offset = 0;
    int32_t w;
    int32_t h;
    int32_t stride;
    uint8_t op;

    if (data == NULL || size < 16) {
        return 0;
    }

    /* Map first bytes to bounded surface dimensions, plus occasional invalid dims */
    w = (int32_t)(data[offset++] % (MAX_W + 4)) - 2;
    h = (int32_t)(data[offset++] % (MAX_H + 4)) - 2;
    stride = w + (int32_t)(data[offset++] % 8) - 2;
    if (stride > MAX_W) {
        stride = MAX_W;
    }

    if (sol_surface_init(&surf, pixels, w, h, stride) != SOL_OK) {
        return 0;
    }

    sol_clear(&surf, SOL_COLOR_BLACK);

    while (offset < size) {
        op = data[offset++] % 10u;
        switch (op) {
        case 0: {
            int32_t cx = read_i32(data, size, &offset);
            int32_t cy = read_i32(data, size, &offset);
            int32_t cw = read_i32(data, size, &offset);
            int32_t ch = read_i32(data, size, &offset);
            sol_surface_set_clip(&surf, SOL_RECT(cx, cy, cw, ch));
            break;
        }
        case 1:
            sol_surface_reset_clip(&surf);
            break;
        case 2: {
            int32_t x = read_i32(data, size, &offset);
            int32_t y = read_i32(data, size, &offset);
            sol_put_pixel(&surf, x, y, read_color(data, size, &offset));
            break;
        }
        case 3: {
            int32_t x = read_i32(data, size, &offset);
            int32_t y = read_i32(data, size, &offset);
            sol_blend_pixel(&surf, x, y, read_color(data, size, &offset));
            break;
        }
        case 4: {
            int32_t x0 = read_i32(data, size, &offset);
            int32_t y0 = read_i32(data, size, &offset);
            int32_t x1 = read_i32(data, size, &offset);
            int32_t y1 = read_i32(data, size, &offset);
            sol_draw_line(&surf, x0, y0, x1, y1, read_color(data, size, &offset));
            break;
        }
        case 5: {
            int32_t rx = read_i32(data, size, &offset);
            int32_t ry = read_i32(data, size, &offset);
            int32_t rw = read_i32(data, size, &offset);
            int32_t rh = read_i32(data, size, &offset);
            sol_fill_rect(&surf, SOL_RECT(rx, ry, rw, rh), read_color(data, size, &offset));
            break;
        }
        case 6: {
            int32_t rx = read_i32(data, size, &offset) % 128;
            int32_t ry = read_i32(data, size, &offset) % 128;
            int32_t rw = read_i32(data, size, &offset) % 128;
            int32_t rh = read_i32(data, size, &offset) % 128;
            int32_t rad = read_i32(data, size, &offset) % 64;
            sol_fill_rounded_rect(&surf, SOL_RECT(rx, ry, rw, rh), rad,
                                  read_color(data, size, &offset));
            break;
        }
        case 7: {
            int32_t cx = read_i32(data, size, &offset) % 128;
            int32_t cy = read_i32(data, size, &offset) % 128;
            int32_t r = read_i32(data, size, &offset) % 64;
            sol_fill_circle(&surf, cx, cy, r, read_color(data, size, &offset));
            break;
        }
        case 8: {
            int32_t cx = read_i32(data, size, &offset) % 128;
            int32_t cy = read_i32(data, size, &offset) % 128;
            int32_t rx = read_i32(data, size, &offset) % 64;
            int32_t ry = read_i32(data, size, &offset) % 64;
            sol_fill_ellipse(&surf, cx, cy, rx, ry, read_color(data, size, &offset));
            break;
        }
        case 9: {
            int32_t x0 = read_i32(data, size, &offset) % 128;
            int32_t y0 = read_i32(data, size, &offset) % 128;
            int32_t x1 = read_i32(data, size, &offset) % 128;
            int32_t y1 = read_i32(data, size, &offset) % 128;
            int32_t x2 = read_i32(data, size, &offset) % 128;
            int32_t y2 = read_i32(data, size, &offset) % 128;
            sol_fill_triangle(&surf, x0, y0, x1, y1, x2, y2, read_color(data, size, &offset));
            break;
        }
        default:
            break;
        }
    }

    return 0;
}

#ifdef SOL_FUZZ_STANDALONE
int main(void) {
    uint8_t buf[128];
    uint32_t state = 0x12345678u;
    size_t iter;
    size_t i;

    /* Empty / short boundary inputs */
    LLVMFuzzerTestOneInput(NULL, 0);
    LLVMFuzzerTestOneInput(buf, 0);
    LLVMFuzzerTestOneInput(buf, 4);

    for (iter = 0; iter < 512; ++iter) {
        size_t len = (iter % sizeof(buf)) + 1;
        for (i = 0; i < len; ++i) {
            state = state * 1664525u + 1013904223u;
            buf[i] = (uint8_t)(state >> 24);
        }
        LLVMFuzzerTestOneInput(buf, len);
    }

    return 0;
}
#endif
