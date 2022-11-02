#include <uv.h>
#include "timer.h"
#include "runtime.h"

struct auto_timer_s
{
    uv_timer_t      timer;
    auto_runtime_t* rt;
    auto_timer_fn   fn;
    void*           arg;
} ;

static void _on_timer_close(uv_handle_t* handle)
{
    auto_timer_t* impl = container_of((uv_timer_t*)handle, auto_timer_t, timer);
    free(impl);
}

static void _timer_on_active(uv_timer_t* handle)
{
    auto_timer_t* impl = container_of(handle, auto_timer_t, timer);
    impl->fn(impl->arg);
}

static void api_timer_destroy(auto_timer_t* self)
{
    uv_close((uv_handle_t*)&self->timer, _on_timer_close);
}

static void api_timer_start(auto_timer_t* self, uint64_t timeout,
                            uint64_t repeat, auto_timer_fn fn, void* arg)
{
    self->fn = fn;
    self->arg = arg;
    uv_timer_start(&self->timer, _timer_on_active, timeout, repeat);
}

static void api_timer_stop(auto_timer_t* self)
{
    uv_timer_stop(&self->timer);
}

static auto_timer_t* api_timer_create(lua_State* L)
{
    auto_runtime_t* rt = auto_get_runtime(L);

    auto_timer_t* impl = malloc(sizeof(auto_timer_t));
    impl->rt = rt;
    uv_timer_init(&rt->loop, &impl->timer);

    return impl;
}

const auto_api_timer_t api_timer = {
    api_timer_create,                   /* .timer.create */
    api_timer_destroy,                  /* .timer.destroy */
    api_timer_start,                    /* .timer.start */
    api_timer_stop,                     /* .timer.stop */
};
