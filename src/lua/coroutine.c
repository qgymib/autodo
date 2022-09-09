#include "runtime.h"
#include "coroutine.h"
#include <string.h>

typedef struct auto_coroutine
{
    auto_runtime_t*     rt;         /**< Global runtime. */
    auto_thread_t*      thr;        /**< Coroutine handle. */
    auto_thread_hook_t  hook;       /**< State change hook. */
    auto_thread_t*      await_thr;  /**< The thread that call await */

    int                 flag_have_result;
    int                 sch_status;
    lua_State*          storage;
    int                 ref_storage;
    int                 n_ret;
} auto_coroutine_t;

static int _coroutine_gc(lua_State* L)
{
    auto_coroutine_t* co = lua_touserdata(L, 1);

    luaL_unref(L, LUA_REGISTRYINDEX, co->ref_storage);
    co->ref_storage = LUA_NOREF;

    return 0;
}

static void _on_coroutine_state_change(auto_thread_t* thr, void* data)
{
    auto_coroutine_t* co = data;
    if (!AUTO_THREAD_IS_DONE(thr))
    {
        auto_thread_hook(&co->hook, thr, _on_coroutine_state_change, co);
        return;
    }

    /* Wakeup */
    if (co->await_thr != NULL)
    {
        auto_set_thread_state(co->rt, co->await_thr, LUA_TNONE);
    }

    co->flag_have_result = 1;
    co->sch_status = thr->data.sch_status;
    co->n_ret = thr->data.n_ret;

    if (AUTO_THREAD_IS_ERROR(thr))
    {/* Push error object on top of stack and raise an error */
        lua_pushvalue(thr->co, -1);
        lua_xmove(thr->co, co->storage, 1);
        return;
    }

    /* Push result on top of stack for storage. */
    int co_sp = lua_gettop(thr->co);
    int co_sp_start = co_sp - thr->data.n_ret + 1;
    for (; co_sp_start <= co_sp; co_sp_start++)
    {
        lua_pushvalue(thr->co, co_sp_start);
    }
    lua_xmove(thr->co, co->storage, co->thr->data.n_ret);
}

static int _coroutine_on_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)status;
    auto_coroutine_t* co = (auto_coroutine_t*)ctx;

    if (co->flag_have_result)
    {/* Finish execution */
        if (co->sch_status != LUA_OK)
        {/* Error occur */
            lua_pushvalue(co->storage, 1);
            lua_xmove(co->storage, L, 1);
            return lua_error(L);
        }

        int i;
        for (i = 1; i <= co->n_ret; i++)
        {
            lua_pushvalue(L, i);
        }
        lua_xmove(co->storage, L, co->n_ret);

        return co->n_ret;
    }

    /* Add to wait_queue */
    auto_set_thread_state(co->rt, co->await_thr, LUA_YIELD);

    /* Yield */
    return lua_yieldk(L, 0, (lua_KContext)co, _coroutine_on_resume);
}

static int _coroutine_await(lua_State* L)
{
    auto_coroutine_t* co = lua_touserdata(L, 1);
    co->await_thr = auto_find_thread(co->rt, L);

    return _coroutine_on_resume(L, LUA_YIELD, (lua_KContext)co);
}

int auto_lua_coroutine(lua_State *L)
{
    int sp = lua_gettop(L);

    auto_coroutine_t* co = lua_newuserdata(L, sizeof(auto_coroutine_t));

    /* Initialize */
    co->rt = auto_get_runtime(L);
    co->thr = auto_new_thread(co->rt, L);
    co->await_thr = NULL;
    co->storage = lua_newthread(L);
    co->ref_storage = luaL_ref(L, LUA_REGISTRYINDEX);
    co->n_ret = 0;
    co->sch_status = 0;
    co->flag_have_result = 0;
    auto_runtime_link(L, sp + 1);

    /* Set metatable */
    static const luaL_Reg s_co_meta[] = {
        { "__gc",   _coroutine_gc },
        { NULL,     NULL },
    };
    static const luaL_Reg s_co_method[] = {
        { "await",  _coroutine_await },
        { NULL,     NULL },
    };
    if (luaL_newmetatable(L, "__auto_coroutine") != 0)
    {
        luaL_setfuncs(L, s_co_meta, 0);
        luaL_newlib(L, s_co_method);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);

    /* Move function and arguments into coroutine */
    int i;
    for (i = 1; i <= sp; i++)
    {
        lua_pushvalue(L, i);
    }
    lua_xmove(L, co->thr->co, sp);

    /* Care about thread state change */
    auto_thread_hook(&co->hook, co->thr, _on_coroutine_state_change, co);

    return 1;
}
