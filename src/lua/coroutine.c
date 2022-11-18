#include "coroutine.h"
#include "runtime.h"
#include "api/list.h"
#include "api/coroutine.h"
#include <string.h>
#include <autodo.h>

typedef struct lua_coroutine
{
    auto_coroutine_t*        thr;        /**< Coroutine handle. */
    auto_coroutine_hook_t*   hook;       /**< State change hook. */
    auto_list_t              wait_queue; /**< Coroutine wait queue */

    int                     flag_have_result;
    int                     flag_closed;

    int                     sch_status;
    lua_State*              storage;
    int                     ref_storage;
    int                     n_ret;
} lua_coroutine_t;

typedef struct lua_wait_record
{
    auto_list_node_t         node;
    struct
    {
        auto_coroutine_t*    wait_coroutine;
        lua_coroutine_t*    belong;
    } data;
} lua_wait_record_t;

static int _coroutine_gc(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);

    if (co->hook != NULL)
    {
        api_coroutine.unhook(co->thr, co->hook);
        co->hook = NULL;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, co->ref_storage);
    co->ref_storage = LUA_NOREF;

    return 0;
}

static void _on_coroutine_state_change(auto_coroutine_t* coroutine, void* arg)
{
    lua_coroutine_t* self = arg;

    /* If the coroutine is not finished, do nothing. */
    if (!(coroutine->status & AUTO_COROUTINE_DEAD))
    {
        return;
    }

    /* Remove the hook */
    api_coroutine.unhook(self->thr, self->hook);
    self->hook = NULL;

    /* Wakeup */
    auto_list_node_t* it = api_list.begin(&self->wait_queue);
    for (; it != NULL; it = api_list.next(it))
    {
        lua_wait_record_t* record = container_of(it, lua_wait_record_t, node);
        api_coroutine.set_state(record->data.wait_coroutine, AUTO_COROUTINE_BUSY);
    }

    self->flag_have_result = 1;
    self->sch_status = coroutine->status;
    self->n_ret = coroutine->nresults;

    if (coroutine->status & AUTO_COROUTINE_ERROR)
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

    if (self->flag_closed)
    {
        lua_pushboolean(L, 0);
        lua_pushnil(L);
        return 2;
    }

    if (!self->flag_have_result)
    {
        api_coroutine.set_state(record->data.wait_coroutine, AUTO_COROUTINE_WAIT);
        return lua_yieldk(L, 0, (lua_KContext)record, _coroutine_on_resume);
    }

    api_list.erase(&self->wait_queue, &record->node);
    api.memory->free(record);

    if (self->sch_status & AUTO_COROUTINE_ERROR)
    {/* Error occur */
        lua_pushboolean(L, 0);
        lua_pushvalue(self->storage, 1);
        lua_xmove(self->storage, L, 1);
        return 2;
    }

    lua_pushboolean(L, 1);

    int i;
    for (i = 1; i <= self->n_ret; i++)
    {
        lua_pushvalue(self->storage, i);
    }
    lua_xmove(self->storage, L, self->n_ret);

    return self->n_ret + 1;
}

static int _coroutine_await(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);

    lua_wait_record_t* record = api.memory->malloc(sizeof(lua_wait_record_t));

    record->data.belong = co;
    record->data.wait_coroutine = api_coroutine.find(L);

    if (record->data.wait_coroutine == NULL)
    {
        api.memory->free(record);
        return api.lua->A_error(L, ERR_HINT_NOT_IN_MANAGED_COROUTINE);
    }

    api_list.push_back(&co->wait_queue, &record->node);

    return _coroutine_on_resume(L, LUA_YIELD, (lua_KContext)record);
}

static int _coroutine_suspend(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);
    api_coroutine.set_state(co->thr, AUTO_COROUTINE_WAIT);
    return 0;
}

static int _coroutine_resume(lua_State* L)
{
    lua_coroutine_t* co = lua_touserdata(L, 1);
    api_coroutine.set_state(co->thr, AUTO_COROUTINE_BUSY);
    return 0;
}

static int _coroutine_close(lua_State* L)
{
    lua_coroutine_t* self = lua_touserdata(L, 1);

    self->flag_closed = 1;
    api_coroutine.set_state(self->thr, AUTO_COROUTINE_DEAD);

    return 0;
}

int auto_new_coroutine(lua_State *L)
{
    int sp = lua_gettop(L);

    lua_coroutine_t* self = lua_newuserdata(L, sizeof(lua_coroutine_t));
    memset(self, 0, sizeof(*self));

    /* Initialize */
    self->thr = api_coroutine.host(lua_newthread(L)); lua_pop(L, 1);
    self->storage = lua_newthread(L);
    self->ref_storage = luaL_ref(L, LUA_REGISTRYINDEX);

    /* Set metatable */
    static const luaL_Reg s_co_meta[] = {
        { "__gc",       _coroutine_gc },
        { NULL,         NULL },
    };
    static const luaL_Reg s_co_method[] = {
        { "await",      _coroutine_await },
        { "close",      _coroutine_close },
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
    self->hook = api_coroutine.hook(self->thr, _on_coroutine_state_change, self);

    return 1;
}
