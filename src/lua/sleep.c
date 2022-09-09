#include <stdlib.h>
#include "runtime.h"
#include "sleep.h"
#include "utils.h"

typedef struct sleep_ctx
{
    uv_timer_t      timer;
    auto_runtime_t* rt;
    auto_thread_t*  u_thr;
} sleep_ctx_t;

static void _on_sleep_timer_close(uv_handle_t* handle)
{
    sleep_ctx_t* ctx = container_of((uv_timer_t*)handle, sleep_ctx_t, timer);
    free(ctx);
}

static void _on_sleep_timer(uv_timer_t* handle)
{
    sleep_ctx_t* ctx = container_of(handle, sleep_ctx_t, timer);

    auto_set_thread_state(ctx->rt, ctx->u_thr, LUA_TNONE);
    uv_close((uv_handle_t*)handle, _on_sleep_timer_close);
}

int auto_lua_sleep(lua_State *L)
{
    auto_runtime_t* rt = auto_get_runtime(L);
    uint32_t timeout = (uint32_t)lua_tointeger(L, -1);

    sleep_ctx_t* ctx = malloc(sizeof(sleep_ctx_t));
    ctx->u_thr = auto_find_thread(rt, L);
    ctx->rt = rt;

    uv_timer_init(&rt->loop, &ctx->timer);
    uv_timer_start(&ctx->timer, _on_sleep_timer, timeout, 0);

    auto_set_thread_state(rt, ctx->u_thr, LUA_YIELD);
    return lua_yield(L, 0);
}
