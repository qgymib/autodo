#include <uv.h>
#include "runtime.h"
#include "notify.h"

struct auto_notify_s
{
    uv_async_t      async;
    auto_runtime_t* rt;
    auto_notify_fn   fn;
    void*           arg;
};

static void _async_on_close(uv_handle_t* handle)
{
    auto_notify_t* impl = container_of((uv_async_t*)handle, auto_notify_t, async);
    free(impl);
}

static void _async_on_active(uv_async_t* handle)
{
    auto_notify_t* impl = container_of(handle, auto_notify_t, async);
    impl->fn(impl->arg);
}

static void api_async_destroy(auto_notify_t* self)
{
    uv_close((uv_handle_t*)&self->async, _async_on_close);
}

static void api_async_send(auto_notify_t* self)
{
    uv_async_send(&self->async);
}

static auto_notify_t* api_async_create(lua_State* L, auto_notify_fn fn, void* arg)
{
    auto_runtime_t* rt = auto_get_runtime(L);

    auto_notify_t* impl = malloc(sizeof(auto_notify_t));

    impl->rt = rt;
    impl->fn = fn;
    impl->arg = arg;
    uv_async_init(&rt->loop, &impl->async, _async_on_active);

    return impl;
}

const auto_api_notify_t api_notify = {
    api_async_create,                   /* .async.create */
    api_async_destroy,                  /* .async.destroy */
    api_async_send,                     /* .async.send */
};
