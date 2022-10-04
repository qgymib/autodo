#include "coroutine.h"
#include "runtime.h"
#include <string.h>
#include <autodo.h>

typedef struct lua_coroutine
{
    atd_coroutine_t*        thr;        /**< Coroutine handle. */
    atd_coroutine_hook_t*   hook;       /**< State change hook. */
    atd_list_t              wait_queue; /**< Coroutine wait queue */

    int                     flag_have_result;
    int                     sch_status;
    lua_State*              storage;
    int                     ref_storage;
    int                     n_ret;
} lua_coroutine_t;

typedef struct lua_wait_record
{
    atd_list_node_t         node;
    struct
    {
        atd_coroutine_t*    wait_coroutine;
        lua_coroutine_t*    belong;
    } data;
} lua_wait_record_t;

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
    lua_coroutine_t* self = arg;

    /* If the coroutine is not finished, do nothing. */
    if (!AUTO_THREAD_IS_DEAD(coroutine))
    {
        return;
    }

    /* Remove the hook */
    api.coroutine.unhook(self->thr, self->hook);
    self->hook = NULL;

    /* Wakeup */
    atd_list_node_t* it = api.list.begin(&self->wait_queue);
    for (; it != NULL; it = api.list.next(it))
    {
        lua_wait_record_t* record = container_of(it, lua_wait_record_t, node);
        api.coroutine.set_state(record->data.wait_coroutine, LUA_TNONE);
    }

    self->flag_have_result = 1;
    self->sch_status = coroutine->status;
    self->n_ret = coroutine->nresults;

    if (AUTO_THREAD_IS_ERROR(coroutine))
    {/* Push error object on top of stack and raise an error */
        lua_pushvalue(coroutine->L, -1);
        lua_xmove(coroutine->L, self->storage, 1);
        return;
    }

    /* Push result on top of stack for storage. */
    int co_sp = lua_gettop(coroutine->L);
    int co_sp_start = co_sp - coroutine->nresults + 1;
    for (; co_sp_start <= co_sp; co_sp_start++)
    {
        lua_pushvalue(coroutine->L, co_sp_start);
    }
    lua_xmove(coroutine->L, self->storage, self->thr->nresults);
}

static int _coroutine_on_resume(lua_State* L, int status, lua_KContext ctx)
{
    (void)status;
    lua_wait_record_t* record = (lua_wait_record_t*)ctx;
    lua_coroutine_t* self = record->data.belong;

    if (self->flag_have_result)
    {/* Finish execution */

        api.list.erase(&self->wait_queue, &record->node);
        free(record);

        if (self->sch_status != LUA_OK)
        {/* Error occur */
            lua_pushvalue(self->storage, 1);
            lua_xmove(self->storage, L, 1);
            return lua_error(L);
        }

        int i;
        for (i = 1; i <= self->n_ret; i++)
        {
            lua_pushvalue(L, i);
        }
        lua_xmove(self->storage, L, self->n_ret);

        return self->n_ret;
    }

    /* Add to wait_queue */
    api.coroutine.set_state(record->data.wait_coroutine, LUA_YIELD);

    /* Yield */
    return lua_yieldk(L, 0, (lua_KContext)record, _coroutine_on_resume);
}

static int _coroutine_await(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);

    lua_wait_record_t* record = malloc(sizeof(lua_wait_record_t));

    record->data.belong = co;
    record->data.wait_coroutine = api.coroutine.find(L);

    if (record->data.wait_coroutine == NULL)
    {
        free(record);
        return luaL_error(L, ERR_HINT_NOT_IN_MANAGED_COROUTINE);
    }

    api.list.push_back(&co->wait_queue, &record->node);

    return _coroutine_on_resume(L, LUA_YIELD, (lua_KContext)record);
}

static int _coroutine_suspend(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);
    api.coroutine.set_state(co->thr, LUA_YIELD);
    return 0;
}

static int _coroutine_resume(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);
    api.coroutine.set_state(co->thr, LUA_TNONE);
    return 0;
}

int auto_new_coroutine(lua_State *L)
{
    int sp = lua_gettop(L);

    lua_coroutine_t* self = lua_newuserdata(L, sizeof(lua_coroutine_t));
    memset(self, 0, sizeof(*self));

    /* Initialize */
    self->thr = api.coroutine.host(lua_newthread(L)); lua_pop(L, 1);
    self->storage = lua_newthread(L);
    self->ref_storage = luaL_ref(L, LUA_REGISTRYINDEX);

    /* Set metatable */
    static const luaL_Reg s_co_meta[] = {
        { "__gc",       _coroutine_gc },
        { NULL,         NULL },
    };
    static const luaL_Reg s_co_method[] = {
        { "await",      _coroutine_await },
        { "suspend",    _coroutine_suspend },
        { "resume",     _coroutine_resume },
        { NULL,         NULL },
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
    lua_xmove(L, self->thr->L, sp);
    self->thr->nresults = sp - 1;

    /* Care about thread state change */
    self->hook = api.coroutine.hook(self->thr, _on_coroutine_state_change, self);

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
