#include "sol_internal.h"

void *sol__memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;

    if (d == NULL || s == NULL || n == 0u) {
        return dst;
    }

    for (i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dst;
}

void *sol__memset(void *dst, int val, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t byte = (uint8_t)val;
    size_t i;

    if (d == NULL || n == 0u) {
        return dst;
    }

    for (i = 0; i < n; ++i) {
        d[i] = byte;
    }
    return dst;
}

void *sol__memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;

    if (d == NULL || s == NULL || n == 0u || d == s) {
        return dst;
    }

    if ((uintptr_t)d < (uintptr_t)s) {
        for (i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        for (i = n; i > 0u; --i) {
            d[i - 1u] = s[i - 1u];
        }
    }
    return dst;
}

int sol__memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    size_t i;

    if (n == 0u || pa == pb) {
        return 0;
    }
    if (pa == NULL) {
        return -1;
    }
    if (pb == NULL) {
        return 1;
    }

    for (i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) {
            return (pa[i] < pb[i]) ? -1 : 1;
        }
    }
    return 0;
}

sol_status_t sol_arena_init(sol_arena_t *arena, void *buffer, size_t capacity) {
    if (arena == NULL || buffer == NULL || capacity == 0u) {
        return SOL_ERR_INVALID_ARG;
    }

    arena->base = (uint8_t *)buffer;
    arena->capacity = capacity;
    arena->offset = 0u;
    return SOL_OK;
}

void *sol_arena_alloc_aligned(sol_arena_t *arena, size_t size, size_t align) {
    uintptr_t curr_addr;
    uintptr_t aligned_addr;
    size_t padding;
    size_t needed;

    if (arena == NULL || arena->base == NULL || size == 0u) {
        return NULL;
    }
    if (align == 0u || (align & (align - 1u)) != 0u) {
        return NULL;
    }

    curr_addr = (uintptr_t)arena->base + (uintptr_t)arena->offset;
    aligned_addr = (curr_addr + (uintptr_t)(align - 1u)) & ~((uintptr_t)(align - 1u));
    padding = (size_t)(aligned_addr - curr_addr);

    if (padding > arena->capacity - arena->offset) {
        return NULL;
    }
    if (size > arena->capacity - arena->offset - padding) {
        return NULL;
    }

    needed = padding + size;
    arena->offset += needed;
    return (void *)aligned_addr;
}

void *sol_arena_alloc(sol_arena_t *arena, size_t size) {
    size_t default_align = sizeof(void *) >= 8u ? sizeof(void *) : 8u;
    return sol_arena_alloc_aligned(arena, size, default_align);
}

void *sol_arena_calloc(sol_arena_t *arena, size_t count, size_t size) {
    size_t total;
    void *ptr;

    if (count == 0u || size == 0u) {
        return NULL;
    }
    if (sol__mul_overflow_sz(count, size, &total)) {
        return NULL;
    }

    ptr = sol_arena_alloc(arena, total);
    if (ptr != NULL) {
        SOL_MEMSET(ptr, 0, total);
    }
    return ptr;
}

void sol_arena_reset(sol_arena_t *arena) {
    if (arena == NULL) {
        return;
    }
    arena->offset = 0u;
}

size_t sol_arena_remaining(const sol_arena_t *arena) {
    if (arena == NULL || arena->offset >= arena->capacity) {
        return 0u;
    }
    return arena->capacity - arena->offset;
}

size_t sol_arena_mark(const sol_arena_t *arena) {
    if (arena == NULL) {
        return 0u;
    }
    return arena->offset;
}

void sol_arena_rewind(sol_arena_t *arena, size_t mark) {
    if (arena == NULL) {
        return;
    }
    if (mark < arena->offset) {
        arena->offset = mark;
    }
}
