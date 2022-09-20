#include "coroutine.h"
#include "runtime.h"
#include <string.h>
#include <autodo.h>

typedef struct lua_coroutine
{
    atd_coroutine_t*        thr;        /**< Coroutine handle. */
    atd_coroutine_hook_t*   hook;       /**< State change hook. */
    atd_coroutine_t*        await_thr;  /**< The thread that call await */

    int                     flag_have_result;
    int                     sch_status;
    lua_State*              storage;
    int                     ref_storage;
    int                     n_ret;
} lua_coroutine_t;

static int _coroutine_gc(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);

    if (co->hook != NULL)
    {
        api.coroutine.unhook(co->thr, co->hook);
        co->hook = NULL;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, co->ref_storage);
    co->ref_storage = LUA_NOREF;

    return 0;
}

static void _on_coroutine_state_change(atd_coroutine_t* coroutine, void* arg)
{
    lua_coroutine_t* co = arg;

    /* If the coroutine is not finished, do nothing. */
    if (!AUTO_THREAD_IS_DEAD(coroutine))
    {
        return;
    }

    /* Remove the hook */
    api.coroutine.unhook(co->thr, co->hook);
    co->hook = NULL;

    /* Wakeup */
    if (co->await_thr != NULL)
    {
        api.coroutine.set_state(co->await_thr, LUA_TNONE);
    }

    co->flag_have_result = 1;
    co->sch_status = coroutine->status;
    co->n_ret = coroutine->nresults;

    if (AUTO_THREAD_IS_ERROR(coroutine))
    {/* Push error object on top of stack and raise an error */
        lua_pushvalue(coroutine->L, -1);
        lua_xmove(coroutine->L, co->storage, 1);
        return;
    }

    /* Push result on top of stack for storage. */
    int co_sp = lua_gettop(coroutine->L);
    int co_sp_start = co_sp - coroutine->nresults + 1;
    for (; co_sp_start <= co_sp; co_sp_start++)
    {
        lua_pushvalue(coroutine->L, co_sp_start);
    }
    lua_xmove(coroutine->L, co->storage, co->thr->nresults);
}

static int _coroutine_on_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)status;
    lua_coroutine_t* co = (lua_coroutine_t*)ctx;

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
    api.coroutine.set_state(co->await_thr, LUA_YIELD);

    /* Yield */
    return lua_yieldk(L, 0, (lua_KContext)co, _coroutine_on_resume);
}

static int _coroutine_await(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);
    co->await_thr = api.coroutine.find(L);

    return _coroutine_on_resume(L, LUA_YIELD, (lua_KContext)co);
}

int atd_lua_coroutine(lua_State *L)
{
    int sp = lua_gettop(L);

    lua_coroutine_t* co = lua_newuserdata(L, sizeof(lua_coroutine_t));
    memset(co, 0, sizeof(*co));

    /* Initialize */
    co->thr = api.coroutine.host(lua_newthread(L)); lua_pop(L, 1);
    co->storage = lua_newthread(L);
    co->ref_storage = luaL_ref(L, LUA_REGISTRYINDEX);

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
    lua_xmove(L, co->thr->L, sp);
    co->thr->nresults = sp - 1;

    /* Care about thread state change */
    co->hook = api.coroutine.hook(co->thr, _on_coroutine_state_change, co);

    return 1;
}

/******************************************************************************
* C API: .coroutine
******************************************************************************/

void api_coroutine_set_state(struct atd_coroutine* self, int state)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    /* In busy_queue */
    if (impl->base.status == LUA_TNONE)
    {
        if (state == LUA_TNONE)
        {/* Do nothing if new state is also BUSY */
            return;
        }

        ev_list_erase(&g_rt->schedule.busy_queue, &impl->q_node);
        impl->base.status = state;
        ev_list_push_back(&g_rt->schedule.wait_queue, &impl->q_node);
        return;
    }

    /* We are in wait_queue, cannot operate on dead coroutine */
    if (impl->base.status != LUA_YIELD)
    {
        abort();
    }

    /* move to busy_queue */
    if (state == LUA_TNONE)
    {
        ev_list_erase(&g_rt->schedule.wait_queue, &impl->q_node);
        impl->base.status = state;
        ev_list_push_back(&g_rt->schedule.busy_queue, &impl->q_node);
    }

    /* thr is dead, keep in wait_queue */
}

atd_coroutine_hook_t* api_coroutine_hook(struct atd_coroutine* self,
    atd_coroutine_hook_fn fn, void* arg)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    atd_coroutine_hook_t* token = malloc(sizeof(atd_coroutine_hook_t));
    token->fn = fn;
    token->arg = arg;
    token->impl = impl;
    ev_list_push_back(&impl->hook.queue, &token->node);

    return token;
}

void api_coroutine_unhook(struct atd_coroutine* self, atd_coroutine_hook_t* token)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    if (impl->hook.it != NULL)
    {
        atd_coroutine_hook_t* next_hook = container_of(impl->hook.it, atd_coroutine_hook_t, node);

        /* If this is next hook, move iterator to next node. */
        if (next_hook == token)
        {
            impl->hook.it = ev_list_next(impl->hook.it);
        }
    }

    ev_list_erase(&impl->hook.queue, &token->node);
    token->impl = NULL;

    free(token);
}

atd_coroutine_t* api_coroutine_host(lua_State* L)
{
    atd_coroutine_impl_t* thr = malloc(sizeof(atd_coroutine_impl_t));

    memset(thr, 0, sizeof(*thr));
    thr->base.L = L;
    thr->base.status = LUA_TNONE;
    ev_list_init(&thr->hook.queue);

    /* Save to schedule table to check duplicate */
    if (ev_map_insert(&g_rt->schedule.all_table, &thr->t_node) != NULL)
    {
        free(thr);
        return NULL;
    }

    /* Get reference */
    lua_pushthread(L);
    thr->data.ref_key = luaL_ref(L, LUA_REGISTRYINDEX);

    /* Save to busy_queue */
    ev_list_push_back(&g_rt->schedule.busy_queue, &thr->q_node);

    return &thr->base;
}

atd_coroutine_t* api_coroutine_find(lua_State* L)
{
    atd_coroutine_impl_t tmp;
    tmp.base.L = L;

    atd_map_node_t* it = ev_map_find(&g_rt->schedule.all_table, &tmp.t_node);
    if (it == NULL)
    {
        return NULL;
    }

    atd_coroutine_impl_t* impl = container_of(it, atd_coroutine_impl_t, t_node);
    return &impl->base;
}
