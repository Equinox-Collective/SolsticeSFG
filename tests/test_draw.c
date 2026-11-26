#include "sol_test.h"

static void test_alpha_blending(void) {
    uint32_t buf[4 * 4];
    sol_surface_t surf;
    sol_color_t half_red = SOL_ARGB(128, 255, 0, 0);
    sol_color_t blended;

    TEST_ASSERT(sol_surface_init(&surf, buf, 4, 4, 4) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    /* Transparent write is a no-op */
    sol_blend_pixel(&surf, 1, 1, SOL_COLOR_TRANSPARENT);
    TEST_ASSERT(sol_get_pixel(&surf, 1, 1) == SOL_COLOR_BLACK);

    /* 50% red over opaque black -> ~128 red, 0 green, 0 blue, 255 alpha */
    sol_blend_pixel(&surf, 1, 1, half_red);
    blended = sol_get_pixel(&surf, 1, 1);
    TEST_ASSERT(SOL_COLOR_A(blended) == 255);
    TEST_ASSERT(SOL_COLOR_R(blended) >= 127 && SOL_COLOR_R(blended) <= 129);
    TEST_ASSERT(SOL_COLOR_G(blended) == 0);
    TEST_ASSERT(SOL_COLOR_B(blended) == 0);
}

static void test_rect_and_square(void) {
    uint32_t buf[16 * 16];
    sol_surface_t surf;

    TEST_ASSERT(sol_surface_init(&surf, buf, 16, 16, 16) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    /* Filled square clipped partly offscreen */
    sol_fill_rect(&surf, SOL_SQUARE(-2, -2, 6), SOL_COLOR_BLUE);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_BLUE);
    TEST_ASSERT(sol_get_pixel(&surf, 3, 3) == SOL_COLOR_BLUE);
    TEST_ASSERT(sol_get_pixel(&surf, 4, 3) == SOL_COLOR_BLACK);
    TEST_ASSERT(sol_get_pixel(&surf, 3, 4) == SOL_COLOR_BLACK);

    /* Outline rect */
    sol_draw_rect(&surf, SOL_RECT(6, 6, 5, 5), SOL_COLOR_GREEN);
    TEST_ASSERT(sol_get_pixel(&surf, 6, 6) == SOL_COLOR_GREEN);
    TEST_ASSERT(sol_get_pixel(&surf, 10, 10) == SOL_COLOR_GREEN);
    TEST_ASSERT(sol_get_pixel(&surf, 8, 8) == SOL_COLOR_BLACK);
}

static void test_lines_and_clipping(void) {
    uint32_t buf[16 * 16];
    sol_surface_t surf;

    TEST_ASSERT(sol_surface_init(&surf, buf, 16, 16, 16) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    /* Diagonal line spanning across entire buffer from far offscreen */
    sol_draw_line(&surf, -100, -100, 100, 100, SOL_COLOR_WHITE);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_WHITE);
    TEST_ASSERT(sol_get_pixel(&surf, 7, 7) == SOL_COLOR_WHITE);
    TEST_ASSERT(sol_get_pixel(&surf, 15, 15) == SOL_COLOR_WHITE);
    TEST_ASSERT(sol_get_pixel(&surf, 7, 8) == SOL_COLOR_BLACK);

    /* Completely offscreen line must touch nothing */
    sol_draw_line(&surf, -50, -10, -1, 20, SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 1) == SOL_COLOR_BLACK);
}

static void test_circle_zero_overdraw(void) {
    uint32_t buf[32 * 32];
    sol_surface_t surf;
    sol_color_t semi_green = SOL_ARGB(128, 0, 255, 0);
    sol_color_t expected_fill;
    int32_t x;
    int32_t y;

    TEST_ASSERT(sol_surface_init(&surf, buf, 32, 32, 32) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    expected_fill = sol_color_blend(SOL_COLOR_BLACK, semi_green);
    sol_fill_circle(&surf, 16, 16, 8, semi_green);

    /* Every pixel inside circle must have identical blended color (no double-drawn seams) */
    for (y = 0; y < 32; ++y) {
        for (x = 0; x < 32; ++x) {
            sol_color_t p = sol_get_pixel(&surf, x, y);
            TEST_ASSERT(p == SOL_COLOR_BLACK || p == expected_fill);
        }
    }
    TEST_ASSERT(sol_get_pixel(&surf, 16, 16) == expected_fill);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_BLACK);
}

static void test_triangle_zero_overdraw(void) {
    uint32_t buf[32 * 32];
    sol_surface_t surf;
    sol_color_t semi_blue = SOL_ARGB(100, 0, 128, 255);
    sol_color_t expected;
    int32_t x;
    int32_t y;

    TEST_ASSERT(sol_surface_init(&surf, buf, 32, 32, 32) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    expected = sol_color_blend(SOL_COLOR_BLACK, semi_blue);
    sol_fill_triangle(&surf, 16, 4, 28, 26, 4, 20, semi_blue);

    for (y = 0; y < 32; ++y) {
        for (x = 0; x < 32; ++x) {
            sol_color_t p = sol_get_pixel(&surf, x, y);
            TEST_ASSERT(p == SOL_COLOR_BLACK || p == expected);
        }
    }
    TEST_ASSERT(sol_get_pixel(&surf, 16, 16) == expected);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_BLACK);
}

static void test_rounded_rect_and_ellipse(void) {
    uint32_t buf[32 * 32];
    sol_surface_t surf;

    TEST_ASSERT(sol_surface_init(&surf, buf, 32, 32, 32) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    sol_fill_rounded_rect(&surf, SOL_RECT(2, 2, 20, 16), 5, SOL_COLOR_ORANGE);
    /* Outer bounding corner (2, 2) must be cut off by rounding */
    TEST_ASSERT(sol_get_pixel(&surf, 2, 2) == SOL_COLOR_BLACK);
    /* Center must be filled */
    TEST_ASSERT(sol_get_pixel(&surf, 12, 10) == SOL_COLOR_ORANGE);

    sol_fill_ellipse(&surf, 20, 22, 8, 4, SOL_COLOR_CYAN);
    TEST_ASSERT(sol_get_pixel(&surf, 20, 22) == SOL_COLOR_CYAN);
    TEST_ASSERT(sol_get_pixel(&surf, 28, 22) == SOL_COLOR_CYAN);
    TEST_ASSERT(sol_get_pixel(&surf, 20, 27) == SOL_COLOR_BLACK);
}

int main(void) {
    test_alpha_blending();
    test_rect_and_square();
    test_lines_and_clipping();
    test_circle_zero_overdraw();
    test_triangle_zero_overdraw();
    test_rounded_rect_and_ellipse();

    TEST_REPORT("test_draw");
    return 0;
}
