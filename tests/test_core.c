#include "sol_test.h"

static void test_surface_init_validation(void) {
    uint32_t buf[16];
    sol_surface_t surf;

    TEST_ASSERT(sol_surface_init(NULL, buf, 4, 4, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, NULL, 4, 4, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, buf, 0, 4, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, buf, 4, -1, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, buf, 4, 4, 3) == SOL_ERR_INVALID_ARG);

    TEST_ASSERT(sol_surface_init(&surf, buf, 4, 4, 4) == SOL_OK);
    TEST_ASSERT(surf.clip.x == 0 && surf.clip.y == 0);
    TEST_ASSERT(surf.clip.w == 4 && surf.clip.h == 4);
}

static void test_clip_and_clear(void) {
    uint32_t buf[4 * 4];
    sol_surface_t surf;
    sol_rect_t clip = {1, 1, 10, 10};
    sol_color_t black = SOL_RGB(0, 0, 0);
    sol_color_t red = SOL_RGB(255, 0, 0);

    TEST_MEMZERO(buf, sizeof(buf));
    TEST_ASSERT(sol_surface_init(&surf, buf, 4, 4, 4) == SOL_OK);

    sol_clear(&surf, black);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == black);

    sol_surface_set_clip(&surf, clip);
    TEST_ASSERT(surf.clip.x == 1 && surf.clip.y == 1);
    TEST_ASSERT(surf.clip.w == 3 && surf.clip.h == 3);

    sol_clear(&surf, red);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == black);
    TEST_ASSERT(sol_get_pixel(&surf, 1, 0) == black);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 1) == black);
    TEST_ASSERT(sol_get_pixel(&surf, 1, 1) == red);
    TEST_ASSERT(sol_get_pixel(&surf, 3, 3) == red);

    /* Out-of-clip and out-of-bounds put_pixel must be safely ignored */
    sol_put_pixel(&surf, 0, 0, red);
    sol_put_pixel(&surf, -5, -5, red);
    sol_put_pixel(&surf, 99, 99, red);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == black);
}

static void test_rect_intersect(void) {
    sol_rect_t a = {-5, -5, 15, 15};
    sol_rect_t b = {0, 0, 8, 8};
    sol_rect_t c = {20, 20, 5, 5};
    sol_rect_t r1 = sol_rect_intersect(a, b);
    sol_rect_t r2 = sol_rect_intersect(b, c);

    TEST_ASSERT(r1.x == 0 && r1.y == 0 && r1.w == 8 && r1.h == 8);
    TEST_ASSERT(r2.w == 0 && r2.h == 0);
}

int main(void) {
    test_surface_init_validation();
    test_clip_and_clear();
    test_rect_intersect();

    TEST_REPORT("test_core");
    return 0;
}
