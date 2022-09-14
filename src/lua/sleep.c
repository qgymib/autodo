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
    ctx->co->set_schedule_state(ctx->co, LUA_TNONE);

    ctx->timer->destroy(ctx->timer);
    ctx->timer = NULL;

    free(ctx);
}

int atd_lua_sleep(lua_State *L)
{
    uint32_t timeout = (uint32_t)lua_tointeger(L, -1);

    sleep_ctx_t* ctx = malloc(sizeof(sleep_ctx_t));
    ctx->co = api.find_coroutine(L);

    ctx->timer = api.new_timer();
    ctx->timer->start(ctx->timer, timeout, 0, _on_sleep_timer, ctx);

    ctx->co->set_schedule_state(ctx->co, LUA_YIELD);
    return lua_yield(L, 0);
}
