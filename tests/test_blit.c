#include "sol_test.h"

static void test_blit_and_self_overlap(void) {
    uint32_t src_buf[4 * 4];
    uint32_t dst_buf[8 * 8];
    sol_surface_t src;
    sol_surface_t dst;
    int32_t i;

    TEST_ASSERT(sol_surface_init(&src, src_buf, 4, 4, 4) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&dst, dst_buf, 8, 8, 8) == SOL_OK);

    for (i = 0; i < 16; ++i) {
        src_buf[i] = SOL_RGB((uint8_t)(i * 10), 200u, 50u);
    }
    sol_clear(&dst, SOL_COLOR_BLACK);

    sol_blit(&dst, 2, 2, &src);
    TEST_ASSERT(sol_get_pixel(&dst, 2, 2) == src_buf[0]);
    TEST_ASSERT(sol_get_pixel(&dst, 5, 5) == src_buf[15]);
    TEST_ASSERT(sol_get_pixel(&dst, 1, 2) == SOL_COLOR_BLACK);

    /* Self-blit scroll down by 1 row must not corrupt overlapped rows */
    sol_blit_rect(&dst, 2, 3, &dst, SOL_RECT(2, 2, 4, 4), SOL_BLEND_NONE);
    TEST_ASSERT(sol_get_pixel(&dst, 2, 3) == src_buf[0]);
    TEST_ASSERT(sol_get_pixel(&dst, 5, 6) == src_buf[15]);
}

static void test_tinted_and_alpha_mask(void) {
    uint32_t src_buf[2 * 2];
    uint32_t dst_buf[4 * 4];
    uint8_t mask[4] = {0u, 128u, 255u, 64u};
    sol_surface_t src;
    sol_surface_t dst;

    TEST_ASSERT(sol_surface_init(&src, src_buf, 2, 2, 2) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&dst, dst_buf, 4, 4, 4) == SOL_OK);

    sol_clear(&src, SOL_COLOR_WHITE);
    sol_clear(&dst, SOL_COLOR_BLACK);

    sol_blit_tinted(&dst, 0, 0, &src, SOL_RECT(0, 0, 2, 2), SOL_RGB(255, 0, 128));
    TEST_ASSERT(sol_get_pixel(&dst, 0, 0) == SOL_RGB(255, 0, 128));

    sol_clear(&dst, SOL_COLOR_BLACK);
    sol_blit_alpha_mask(&dst, 1, 1, mask, 2, 2, 2, SOL_RECT(0, 0, 2, 2), SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&dst, 1, 1) == SOL_COLOR_BLACK);
    TEST_ASSERT(SOL_COLOR_R(sol_get_pixel(&dst, 2, 1)) >= 127u &&
                SOL_COLOR_R(sol_get_pixel(&dst, 2, 1)) <= 129u);
    TEST_ASSERT(sol_get_pixel(&dst, 1, 2) == SOL_COLOR_RED);
}

static void test_9slice_and_scale(void) {
    uint32_t src_buf[6 * 6];
    uint32_t dst_buf[12 * 12];
    sol_surface_t src;
    sol_surface_t dst;

    TEST_ASSERT(sol_surface_init(&src, src_buf, 6, 6, 6) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&dst, dst_buf, 12, 12, 12) == SOL_OK);

    sol_clear(&src, SOL_COLOR_GREEN);
    sol_draw_rect(&src, SOL_RECT(0, 0, 6, 6), SOL_COLOR_RED);
    sol_clear(&dst, SOL_COLOR_BLACK);

    sol_blit_9slice(&dst, SOL_RECT(0, 0, 12, 12), &src, SOL_RECT(0, 0, 6, 6),
                    SOL_9SLICE(2, 2, 2, 2), false, SOL_BLEND_NONE);
    TEST_ASSERT(sol_get_pixel(&dst, 0, 0) == SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&dst, 11, 11) == SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&dst, 6, 6) == SOL_COLOR_GREEN);
}

static void test_transform_flip_and_rotate(void) {
    uint32_t src_buf[3 * 2];
    uint32_t rot_buf[2 * 3];
    uint32_t dst_buf[16 * 16];
    sol_surface_t src;
    sol_surface_t rot;
    sol_surface_t dst;
    sol_mat2x3_t mat;

    TEST_ASSERT(sol_surface_init(&src, src_buf, 3, 2, 3) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&rot, rot_buf, 2, 3, 2) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&dst, dst_buf, 16, 16, 16) == SOL_OK);

    sol_clear(&src, SOL_COLOR_BLUE);
    sol_put_pixel(&src, 0, 0, SOL_COLOR_RED);

    TEST_ASSERT(sol_surface_rotate_90(&rot, &src, 1) == SOL_OK);
    TEST_ASSERT(sol_get_pixel(&rot, 1, 0) == SOL_COLOR_RED);

    sol_surface_flip_h(&src);
    TEST_ASSERT(sol_get_pixel(&src, 2, 0) == SOL_COLOR_RED);
    sol_surface_flip_v(&src);
    TEST_ASSERT(sol_get_pixel(&src, 2, 1) == SOL_COLOR_RED);

    sol_clear(&dst, SOL_COLOR_BLACK);
    mat = sol_mat2x3_mul(sol_mat2x3_translate(8, 4), sol_mat2x3_rotate_deg(90));
    sol_blit_transformed(&dst, &src, SOL_RECT(0, 0, 3, 2), mat, false, SOL_BLEND_NONE);
    TEST_ASSERT(sol_get_pixel(&dst, 7, 6) == SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&dst, 8, 4) == SOL_COLOR_BLUE);
    TEST_ASSERT(sol_get_pixel(&dst, 6, 6) == SOL_COLOR_BLACK);
}

int main(void) {
    test_blit_and_self_overlap();
    test_tinted_and_alpha_mask();
    test_9slice_and_scale();
    test_transform_flip_and_rotate();

    TEST_REPORT("test_blit");
    return 0;
}
