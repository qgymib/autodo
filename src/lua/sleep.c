#include <autodo.h>
#include <string.h>
#include "api/timer.h"
#include "api/coroutine.h"
#include "sleep.h"

typedef struct sleep_ctx
{
    auto_timer_t*       timer;  /**< Timer handle. */
    auto_coroutine_t*   co;     /**< Coroutine handle. */
} sleep_ctx_t;

static void _sleep_release_timer(sleep_ctx_t* ctx)
{
    if (ctx->timer != NULL)
    {
        api_timer.destroy(ctx->timer);
        ctx->timer = NULL;
    }
}

static void _on_sleep_timer(void* arg)
{
    sleep_ctx_t* ctx = arg;
    api_coroutine.set_state(ctx->co, LUA_TNONE);

    _sleep_release_timer(ctx);
}

static int _sleep_gc(lua_State* L)
{
    sleep_ctx_t* ctx = lua_touserdata(L, 1);

    _sleep_release_timer(ctx);

    return 0;
}

static void _sleep_set_metatable(lua_State* L)
{
    static const luaL_Reg s_sleep_meta[] = {
        { "__gc",       _sleep_gc },
        { NULL,         NULL },
    };
    if (luaL_newmetatable(L, "__auto_sleep") != 0)
    {
        luaL_setfuncs(L, s_sleep_meta, 0);
    }
    lua_setmetatable(L, -2);
}

static int _sleep_on_resume(lua_State* L, int status, lua_KContext ctx)
{
    (void)status; (void)ctx;

    lua_pop(L, 1);
    return 0;
}

int atd_lua_sleep(lua_State* L)
{
    uint64_t timeout = (uint64_t)lua_tointeger(L, -1);

    sleep_ctx_t* ctx = lua_newuserdata(L, sizeof(sleep_ctx_t));

    memset(ctx, 0, sizeof(*ctx));
    _sleep_set_metatable(L);

    ctx->co = api_coroutine.find(L);
    ctx->timer = api_timer.create(L);
    api_timer.start(ctx->timer, timeout, 0, _on_sleep_timer, ctx);

    api_coroutine.set_state(ctx->co, LUA_YIELD);

    return lua_yieldk(L, 0, (lua_KContext)ctx, _sleep_on_resume);
}
