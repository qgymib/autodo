#include <stdlib.h>
#include <autodo.h>
#include "sleep.h"

typedef struct sleep_ctx
{
    atd_timer_t*        timer;
    atd_coroutine_t*    co;
} sleep_ctx_t;

static void _on_sleep_timer(void* arg)
{
    sleep_ctx_t* ctx = arg;
    auto_api()->coroutine.set_state(ctx->co, LUA_TNONE);

    auto_api()->timer.destroy(ctx->timer);
    ctx->timer = NULL;

    free(ctx);
}

int atd_lua_sleep(lua_State *L)
{
    uint32_t timeout = (uint32_t)lua_tointeger(L, -1);

    sleep_ctx_t* ctx = malloc(sizeof(sleep_ctx_t));
    ctx->co = auto_api()->coroutine.find(L);

    ctx->timer = auto_api()->timer.create();
    auto_api()->timer.start(ctx->timer, timeout, 0, _on_sleep_timer, ctx);

    auto_api()->coroutine.set_state(ctx->co, LUA_YIELD);
    return lua_yield(L, 0);
}
