#include "sol_test.h"

static void test_freestanding_mem_ops(void) {
    uint8_t buf[16];
    uint8_t cmp[16];
    size_t i;

    sol__memset(buf, 0xAA, sizeof(buf));
    for (i = 0; i < sizeof(buf); ++i) {
        TEST_ASSERT(buf[i] == 0xAAu);
    }

    for (i = 0; i < sizeof(cmp); ++i) {
        cmp[i] = (uint8_t)i;
    }
    sol__memcpy(buf, cmp, sizeof(cmp));
    TEST_ASSERT(sol__memcmp(buf, cmp, sizeof(cmp)) == 0);

    /* Overlapping forward move: shift right by 4 */
    sol__memmove(buf + 4, buf, 8);
    TEST_ASSERT(buf[4] == 0 && buf[5] == 1 && buf[11] == 7);

    /* Overlapping backward move: shift left by 2 */
    sol__memmove(buf + 2, buf + 4, 8);
    TEST_ASSERT(buf[2] == 0 && buf[9] == 7);

    buf[0] = 1;
    cmp[0] = 2;
    TEST_ASSERT(sol__memcmp(buf, cmp, 1) < 0);
    TEST_ASSERT(sol__memcmp(cmp, buf, 1) > 0);
}

static void test_arena_allocator(void) {
    uint8_t backing[256];
    sol_arena_t arena;
    void *p1;
    void *p2;
    uint32_t *arr;
    size_t mark;
    size_t i;

    TEST_ASSERT(sol_arena_init(NULL, backing, sizeof(backing)) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_arena_init(&arena, NULL, sizeof(backing)) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_arena_init(&arena, backing, 0u) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_arena_init(&arena, backing, sizeof(backing)) == SOL_OK);

    /* Non-power-of-two alignment or zero size must fail */
    TEST_ASSERT(sol_arena_alloc_aligned(&arena, 16u, 3u) == NULL);
    TEST_ASSERT(sol_arena_alloc(&arena, 0u) == NULL);

    p1 = sol_arena_alloc_aligned(&arena, 13u, 16u);
    TEST_ASSERT(p1 != NULL);
    TEST_ASSERT(((uintptr_t)p1 & 15u) == 0u);

    mark = sol_arena_mark(&arena);
    arr = (uint32_t *)sol_arena_calloc(&arena, 8u, sizeof(uint32_t));
    TEST_ASSERT(arr != NULL);
    for (i = 0; i < 8u; ++i) {
        TEST_ASSERT(arr[i] == 0u);
    }

    /* Overflow in calloc must return NULL */
    TEST_ASSERT(sol_arena_calloc(&arena, (size_t)-1, 4u) == NULL);

    /* Rewind restores remaining capacity */
    sol_arena_rewind(&arena, mark);
    p2 = sol_arena_alloc(&arena, 16u);
    TEST_ASSERT(p2 != NULL);

    /* OOM returns NULL */
    TEST_ASSERT(sol_arena_alloc(&arena, 1024u) == NULL);

    sol_arena_reset(&arena);
    TEST_ASSERT(sol_arena_remaining(&arena) == sizeof(backing));
}

static void test_pixel_format_conversions(void) {
    uint32_t src[2];
    uint8_t out[16];
    uint16_t out16[2];
    uint32_t out32[2];

    /* 0xAARRGGBB: A=0x80, R=0xFF, G=0x40, B=0x20 */
    src[0] = SOL_ARGB(0x80u, 0xFFu, 0x40u, 0x20u);
    src[1] = SOL_ARGB(0xFFu, 0x00u, 0xFFu, 0x00u);

    TEST_ASSERT(sol_format_bpp(SOL_FMT_ARGB8888) == 4);
    TEST_ASSERT(sol_format_bpp(SOL_FMT_RGB888) == 3);
    TEST_ASSERT(sol_format_bpp(SOL_FMT_RGB565) == 2);
    TEST_ASSERT(sol_format_bpp(SOL_FMT_GRAY8) == 1);

    TEST_ASSERT(sol_convert_row(out32, SOL_FMT_XRGB8888, src, 2) == SOL_OK);
    TEST_ASSERT(out32[0] == 0xFFFF4020u);

    TEST_ASSERT(sol_convert_row(out32, SOL_FMT_RGBA8888, src, 2) == SOL_OK);
    TEST_ASSERT(out32[0] == 0xFF402080u);

    TEST_ASSERT(sol_convert_row(out32, SOL_FMT_BGRA8888, src, 2) == SOL_OK);
    TEST_ASSERT(out32[0] == 0x2040FF80u);

    TEST_ASSERT(sol_convert_row(out32, SOL_FMT_ABGR8888, src, 2) == SOL_OK);
    TEST_ASSERT(out32[0] == 0x802040FFu);

    TEST_ASSERT(sol_convert_row(out, SOL_FMT_RGB888, src, 2) == SOL_OK);
    TEST_ASSERT(out[0] == 0xFFu && out[1] == 0x40u && out[2] == 0x20u);

    TEST_ASSERT(sol_convert_row(out, SOL_FMT_BGR888, src, 2) == SOL_OK);
    TEST_ASSERT(out[0] == 0x20u && out[1] == 0x40u && out[2] == 0xFFu);

    TEST_ASSERT(sol_convert_row(out16, SOL_FMT_RGB565, src, 2) == SOL_OK);
    TEST_ASSERT(out16[0] == (uint16_t)((31u << 11) | (16u << 5) | 4u));
    TEST_ASSERT(out16[1] == (uint16_t)(63u << 5));

    TEST_ASSERT(sol_convert_row(out, SOL_FMT_A8, src, 2) == SOL_OK);
    TEST_ASSERT(out[0] == 0x80u && out[1] == 0xFFu);

    TEST_ASSERT(sol_convert_row(out, SOL_FMT_GRAY8, src, 2) == SOL_OK);
    TEST_ASSERT(out[1] == 149u); /* (150 * 255 + 128) >> 8 == 149 */
}

static void test_present_and_dither(void) {
    uint32_t src_buf[4 * 2];
    uint16_t dst_buf[6 * 2]; /* Padded stride: 12 bytes/row */
    sol_surface_t surf;
    size_t i;

    TEST_ASSERT(sol_surface_init(&surf, src_buf, 4, 2, 4) == SOL_OK);
    for (i = 0; i < 8u; ++i) {
        src_buf[i] = SOL_RGB(0x04u, 0x02u, 0x04u);
    }

    TEST_MEMZERO(dst_buf, sizeof(dst_buf));
    TEST_ASSERT(sol_present_to_buffer(dst_buf, 6, SOL_FMT_RGB565, &surf, true) ==
                SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_present_to_buffer(dst_buf, 12, SOL_FMT_RGB565, &surf, true) == SOL_OK);

    /* Padding columns (4, 5) on each row must remain untouched */
    TEST_ASSERT(dst_buf[4] == 0u && dst_buf[5] == 0u);
    TEST_ASSERT(dst_buf[10] == 0u && dst_buf[11] == 0u);

    /* Dithering mid-step values produces a mix of 0 and non-zero quantized pixels */
    TEST_ASSERT(dst_buf[0] == 0u);
    TEST_ASSERT(dst_buf[1] != 0u);
}

int main(void) {
    test_freestanding_mem_ops();
    test_arena_allocator();
    test_pixel_format_conversions();
    test_present_and_dither();

    TEST_REPORT("test_alloc_format");
    return 0;
}
