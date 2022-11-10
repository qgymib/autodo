#include <uv.h>
#include "thread.h"

struct auto_thread_s
{
    uv_thread_t     thread;
};

static void api_thread_join(auto_thread_t* self)
{
    uv_thread_join(&self->thread);
    api.memory->free(self);
}

static auto_thread_t* api_thread_create(auto_thread_fn fn, void* arg)
{
    auto_thread_t* impl = api.memory->malloc(sizeof(auto_thread_t));
    if (uv_thread_create(&impl->thread, fn, arg) != 0)
    {
        api.memory->free(impl);
        return NULL;
    }
    return impl;
}

const auto_api_thread_t api_thread = {
    api_thread_create,                  /* .thread.create */
    api_thread_join,                    /* .thread.join */
    uv_sleep,                           /* .thread.sleep */
};
