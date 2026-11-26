#include "sol_test.h"

static void test_blend_modes(void) {
    sol_color_t red = SOL_RGB(200, 0, 0);
    sol_color_t half_white = SOL_ARGB(128, 255, 255, 255);
    sol_color_t half_blue = SOL_ARGB(128, 0, 0, 200);
    sol_color_t c;

    /* NONE overwrites completely, even with alpha 0 */
    TEST_ASSERT(sol_color_blend_mode(red, SOL_COLOR_TRANSPARENT, SOL_BLEND_NONE) ==
                SOL_COLOR_TRANSPARENT);

    /* ADD saturates cleanly at 255 */
    c = sol_color_blend_mode(SOL_RGB(200, 10, 10), SOL_RGB(100, 20, 0), SOL_BLEND_ADD);
    TEST_ASSERT(SOL_COLOR_R(c) == 255 && SOL_COLOR_G(c) == 30 && SOL_COLOR_B(c) == 10);

    /* MULTIPLY with white leaves color unchanged; with black turns black */
    TEST_ASSERT(sol_color_blend_mode(red, SOL_COLOR_WHITE, SOL_BLEND_MULTIPLY) == red);
    TEST_ASSERT(sol_color_blend_mode(red, SOL_COLOR_BLACK, SOL_BLEND_MULTIPLY) == SOL_COLOR_BLACK);

    /* SCREEN with black leaves color unchanged; with white turns white */
    TEST_ASSERT(sol_color_blend_mode(red, SOL_COLOR_BLACK, SOL_BLEND_SCREEN) == red);
    TEST_ASSERT(sol_color_blend_mode(red, SOL_COLOR_WHITE, SOL_BLEND_SCREEN) == SOL_COLOR_WHITE);

    /* SRC_IN & SRC_ATOP preserve expected alpha semantics */
    c = sol_color_blend_mode(half_blue, half_white, SOL_BLEND_SRC_ATOP);
    TEST_ASSERT(SOL_COLOR_A(c) == 128);

    c = sol_color_blend_mode(SOL_COLOR_TRANSPARENT, half_white, SOL_BLEND_SRC_IN);
    TEST_ASSERT(SOL_COLOR_A(c) == 0);
}

static void test_premultiplied_alpha(void) {
    sol_color_t straight = SOL_ARGB(128, 200, 100, 50);
    sol_color_t pm = sol_color_premul(straight);
    sol_color_t back = sol_color_unpremul(pm);
    sol_color_t out;

    TEST_ASSERT(SOL_COLOR_A(pm) == 128);
    TEST_ASSERT(SOL_COLOR_R(pm) >= 99 && SOL_COLOR_R(pm) <= 101);
    TEST_ASSERT(SOL_COLOR_G(pm) >= 49 && SOL_COLOR_G(pm) <= 51);
    TEST_ASSERT(SOL_COLOR_R(back) >= 198 && SOL_COLOR_R(back) <= 202);

    TEST_ASSERT(sol_color_premul(SOL_COLOR_TRANSPARENT) == 0u);
    TEST_ASSERT(sol_color_unpremul(SOL_COLOR_TRANSPARENT) == 0u);

    out = sol_color_blend_premul(SOL_COLOR_BLACK, pm);
    TEST_ASSERT(SOL_COLOR_A(out) == 255);
    TEST_ASSERT(SOL_COLOR_R(out) == SOL_COLOR_R(pm));
}

static void test_srgb_linear_roundtrip_and_wcag(void) {
    uint32_t i;
    uint32_t prev = 0u;

    /* Every sRGB 0..255 must roundtrip bit-for-bit and be strictly increasing */
    for (i = 0u; i <= 255u; ++i) {
        uint16_t lin = sol_srgb_to_linear((uint8_t)i);
        uint8_t back = sol_linear_to_srgb(lin);
        TEST_ASSERT(back == (uint8_t)i);
        if (i > 0u) {
            TEST_ASSERT(lin > prev);
        }
        prev = lin;
    }

    TEST_ASSERT(sol_srgb_to_linear(0u) == 0u);
    TEST_ASSERT(sol_srgb_to_linear(255u) == 65535u);

    /* WCAG contrast ratios */
    TEST_ASSERT(sol_color_contrast_x100(SOL_COLOR_BLACK, SOL_COLOR_WHITE) == 2100u);
    TEST_ASSERT(sol_color_contrast_x100(SOL_COLOR_RED, SOL_COLOR_RED) == 100u);
    TEST_ASSERT(sol_color_luminance(SOL_COLOR_WHITE) == 255u);
    TEST_ASSERT(sol_color_luminance(SOL_COLOR_BLACK) == 0u);
}

static void test_hsv_hsl_and_hex(void) {
    uint16_t h = 0;
    uint8_t s = 0;
    uint8_t v = 0;
    uint8_t l = 0;
    sol_color_t c;
    char hex_buf[16];

    /* HSV pure red / green / blue / yellow */
    c = sol_color_from_hsv(120u, 255u, 255u, 255u);
    TEST_ASSERT(c == SOL_COLOR_GREEN);
    sol_color_to_hsv(c, &h, &s, &v);
    TEST_ASSERT(h == 120u && s == 255u && v == 255u);

    /* HSL pure cyan (h=180, s=255, l=128 -> R=1 due to 128 > 127.5) */
    c = sol_color_from_hsl(180u, 255u, 128u, 255u);
    TEST_ASSERT(SOL_COLOR_R(c) <= 1 && SOL_COLOR_G(c) >= 254 && SOL_COLOR_B(c) >= 254);
    c = sol_color_from_hsl(180u, 255u, 127u, 255u);
    TEST_ASSERT(SOL_COLOR_R(c) == 0 && SOL_COLOR_G(c) >= 254 && SOL_COLOR_B(c) >= 254);
    sol_color_to_hsl(SOL_COLOR_CYAN, &h, &s, &l);
    TEST_ASSERT(h == 180u && s == 255u && l == 128u);

    /* Hex parsing & formatting */
    TEST_ASSERT(sol_color_from_hex("#F80", &c) == SOL_OK);
    TEST_ASSERT(c == SOL_RGB(0xFFu, 0x88u, 0x00u));

    TEST_ASSERT(sol_color_from_hex("#80112233", &c) == SOL_OK);
    TEST_ASSERT(c == SOL_ARGB(0x80u, 0x11u, 0x22u, 0x33u));
    TEST_ASSERT(sol_color_to_hex(c, hex_buf, sizeof(hex_buf), true) == SOL_OK);
    TEST_ASSERT(sol__memcmp(hex_buf, "#80112233", 10u) == 0);
    TEST_ASSERT(sol_color_to_hex(c, hex_buf, 4u, false) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_color_from_hex("#ZZZZZZ", &c) == SOL_ERR_INVALID_ARG);
}

static void test_gradients(void) {
    uint32_t buf[16 * 16];
    sol_surface_t surf;
    sol_gradient_stop_t stops[3];
    sol_color_t c_mid;

    stops[0].pos = 0u;
    stops[0].color = SOL_COLOR_RED;
    stops[1].pos = 128u;
    stops[1].color = SOL_COLOR_GREEN;
    stops[2].pos = 255u;
    stops[2].color = SOL_COLOR_BLUE;

    TEST_ASSERT(sol_gradient_eval(stops, 3u, 0u, false) == SOL_COLOR_RED);
    c_mid = sol_gradient_eval(stops, 3u, 128u, false);
    TEST_ASSERT(c_mid == SOL_COLOR_GREEN);
    TEST_ASSERT(sol_gradient_eval(stops, 3u, 255u, false) == SOL_COLOR_BLUE);

    TEST_ASSERT(sol_surface_init(&surf, buf, 16, 16, 16) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    sol_fill_rect_gradient_4(&surf, SOL_RECT(0, 0, 16, 16), SOL_COLOR_RED, SOL_COLOR_GREEN,
                             SOL_COLOR_WHITE, SOL_COLOR_BLUE);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&surf, 15, 0) == SOL_COLOR_GREEN);
    TEST_ASSERT(sol_get_pixel(&surf, 15, 15) == SOL_COLOR_WHITE);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 15) == SOL_COLOR_BLUE);

    sol_fill_rect_gradient_radial(&surf, SOL_RECT(0, 0, 16, 16), 8, 8, 8, stops, 3u, true);
    TEST_ASSERT(sol_get_pixel(&surf, 8, 8) == SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_BLUE);
}

int main(void) {
    test_blend_modes();
    test_premultiplied_alpha();
    test_srgb_linear_roundtrip_and_wcag();
    test_hsv_hsl_and_hex();
    test_gradients();

    TEST_REPORT("test_blend");
    return 0;
}
