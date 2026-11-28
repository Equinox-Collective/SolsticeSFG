/*
 * SolsticeSFG - Image Decoders Fuzz Target (PNG, QOI, BMP, PPM, TGA)
 * SPDX-License-Identifier: Zlib OR MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "solsticesfg.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static uint8_t arena_buf[256u * 1024u];
    sol_arena_t arena;
    sol_image_t img;

    if (data == NULL || size == 0u) {
        return 0;
    }

    if (sol_arena_init(&arena, arena_buf, sizeof(arena_buf)) == SOL_OK) {
        sol_image_load_mem(&img, data, size, &arena);
    }
    if (sol_arena_init(&arena, arena_buf, sizeof(arena_buf)) == SOL_OK) {
        sol_image_load_tga_mem(&img, data, size, &arena);
    }
    return 0;
}

#ifdef SOL_FUZZ_STANDALONE
int main(void) {
    uint8_t buf[256];
    uint32_t state = 0xCAFEBABEu;
    size_t iter;
    size_t i;

    LLVMFuzzerTestOneInput(NULL, 0u);
    LLVMFuzzerTestOneInput(buf, 0u);

    for (iter = 0u; iter < 512u; ++iter) {
        size_t len = (iter % sizeof(buf)) + 1u;
        for (i = 0u; i < len; ++i) {
            state = state * 1664525u + 1013904223u;
            buf[i] = (uint8_t)(state >> 24);
        }
        LLVMFuzzerTestOneInput(buf, len);
    }
    return 0;
}
#endif
