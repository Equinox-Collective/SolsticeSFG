#ifndef SOL_INTERNAL_H
#define SOL_INTERNAL_H

#include "../include/solsticesfg.h"

#ifndef SOL_PRIVATE
    #define SOL_PRIVATE static
#endif

#define SOL_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SOL_MAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* SOL_INTERNAL_H */
