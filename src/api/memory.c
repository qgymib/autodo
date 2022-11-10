#include "memory.h"
#include <stdlib.h>

static void* _mem_malloc(size_t size)
{
    return malloc(size);
}

static void _mem_free(void* ptr)
{
    free(ptr);
}

static void* _mem_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void* _mem_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

const auto_api_memory_t api_memory = {
    _mem_malloc,    /* .mem.malloc */
    _mem_free,      /* .mem.free */
    _mem_calloc,    /* .mem.calloc */
    _mem_realloc,   /* .mem.realloc */
};
