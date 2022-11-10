#include <uv.h>
#include "misc.h"
#include "utils.h"

static ssize_t api_search(const void* data, size_t size, const void* key, size_t len)
{
    int32_t* fsm = api.memory->malloc(sizeof(int) * len);
    ssize_t ret = aeda_find(data, size, key, len, fsm, len);
    api.memory->free(fsm);
    return ret;
}

const auto_api_misc_t api_misc = {
    uv_hrtime,                          /* .misc.hrtime */
    api_search,                         /* .misc.search */
};
