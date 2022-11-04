#include "memory.h"

const auto_api_memory_t api_memory = {
    malloc,                             /* .mem.malloc */
    free,                               /* .mem.free */
    calloc,                             /* .mem.calloc */
    realloc,                            /* .mem.realloc */
};
