#include "sol_test.h"

static void test_aa_primitives(void) {
    uint32_t buf[32 * 32];
    sol_surface_t surf;
    sol_color_t mid;

    TEST_ASSERT(sol_surface_init(&surf, buf, 32, 32, 32) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    /* AA rounded rect: corner pixel must have partial/zero coverage, center solid */
    sol_fill_rounded_rect_ex_aa(&surf, SOL_RECT(4, 4, 24, 24), SOL_CORNERS(8, 0, 8, 0),
                                SOL_COLOR_WHITE);
    TEST_ASSERT(sol_get_pixel(&surf, 16, 16) == SOL_COLOR_WHITE);
    TEST_ASSERT(sol_get_pixel(&surf, 27, 4) == SOL_COLOR_WHITE); /* Sharp TR corner (r=0) */
    TEST_ASSERT(sol_get_pixel(&surf, 4, 4) == SOL_COLOR_BLACK);  /* Rounded TL corner cut */

    /* AA line produces smooth intermediate pixel shades */
    sol_clear(&surf, SOL_COLOR_BLACK);
    sol_draw_line_aa(&surf, 2, 2, 29, 10, SOL_COLOR_WHITE);
    mid = sol_get_pixel(&surf, 2, 2);
    TEST_ASSERT(SOL_COLOR_R(mid) > 200u);

    /* Thick AA line with round caps */
    sol_clear(&surf, SOL_COLOR_BLACK);
    sol_draw_line_thick_aa(&surf, 8, 16, 24, 16, 6, SOL_CAP_ROUND, SOL_COLOR_CYAN);
    TEST_ASSERT(sol_get_pixel(&surf, 16, 16) == SOL_COLOR_CYAN);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_BLACK);
}

static void test_ring_pie_and_dashed(void) {
    uint32_t buf[32 * 32];
    sol_surface_t surf;

    TEST_ASSERT(sol_surface_init(&surf, buf, 32, 32, 32) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    sol_fill_ring(&surf, 16, 16, 5, 10, SOL_COLOR_GREEN);
    TEST_ASSERT(sol_get_pixel(&surf, 16, 16) == SOL_COLOR_BLACK); /* Hollow center */
    TEST_ASSERT(sol_get_pixel(&surf, 24, 16) == SOL_COLOR_GREEN); /* Ring band */

    sol_clear(&surf, SOL_COLOR_BLACK);
    sol_draw_line_dashed(&surf, 0, 5, 20, 5, 4, 4, SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 5) == SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&surf, 4, 5) == SOL_COLOR_BLACK);
    TEST_ASSERT(sol_get_pixel(&surf, 8, 5) == SOL_COLOR_RED);
}

static void test_blur_shadow_and_noise(void) {
    uint32_t buf[32 * 32];
    sol_surface_t surf;
    sol_color_t edge_px;

    TEST_ASSERT(sol_surface_init(&surf, buf, 32, 32, 32) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    /* Sharp white square blurred in place spreads light into neighbors */
    sol_fill_rect(&surf, SOL_RECT(12, 12, 8, 8), SOL_COLOR_WHITE);
    sol_blur_rect(&surf, SOL_RECT(4, 4, 24, 24), 4);
    edge_px = sol_get_pixel(&surf, 10, 16);
    TEST_ASSERT(SOL_COLOR_R(edge_px) > 0u && SOL_COLOR_R(edge_px) < 255u);

    /* Box shadow fades out smoothly outside box */
    sol_clear(&surf, SOL_COLOR_WHITE);
    sol_draw_box_shadow(&surf, SOL_RECT(10, 10, 12, 12), 4, 0, 0, 4, 0, SOL_ARGB(200, 0, 0, 0));
    TEST_ASSERT(SOL_COLOR_R(sol_get_pixel(&surf, 16, 16)) < 100u);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_WHITE);

    /* Noise changes flat black without crashing */
    sol_clear(&surf, SOL_RGB(64, 64, 64));
    sol_apply_noise(&surf, SOL_RECT(0, 0, 32, 32), 10u, 42u);
    TEST_ASSERT(SOL_COLOR_R(sol_get_pixel(&surf, 15, 15)) >= 54u &&
                SOL_COLOR_R(sol_get_pixel(&surf, 15, 15)) <= 74u);
}

int main(void) {
    test_aa_primitives();
    test_ring_pie_and_dashed();
    test_blur_shadow_and_noise();

    TEST_REPORT("test_aa_fx");
    return 0;
}
