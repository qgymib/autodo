#include <string.h>
#include "coroutine.h"
#include "runtime.h"

/**
 * @brief Host Lua coroutine as managed coroutine.
 * @param[in] L Lua coroutine.
 * @return      Managed coroutine context.
 */
static auto_coroutine_t* _coroutine_host(lua_State* L)
{
    auto_runtime_t* rt = auto_get_runtime(L);
    atd_coroutine_impl_t* thr = malloc(sizeof(atd_coroutine_impl_t));

    memset(thr, 0, sizeof(*thr));
    thr->rt = rt;
    thr->base.L = L;
    thr->base.status = AUTO_COROUTINE_BUSY;
    ev_list_init(&thr->hook.queue);

    /* Save to schedule table to check duplicate */
    if (ev_map_insert(&rt->schedule.all_table, &thr->t_node) != NULL)
    {
        free(thr);
        return NULL;
    }

    /* Get reference */
    lua_pushthread(L);
    thr->data.ref_key = luaL_ref(L, LUA_REGISTRYINDEX);

    /* Save to busy_queue */
    ev_list_push_back(&rt->schedule.busy_queue, &thr->q_node);

    return &thr->base;
}

/**
 * @brief Find coroutine context by \p L.
 * @internal
 * @param[in] L Lua coroutine.
 * @return      Coroutine context.
 */
static auto_coroutine_t* _coroutine_find(lua_State* L)
{
    auto_runtime_t* rt = auto_get_runtime(L);

    atd_coroutine_impl_t tmp;
    tmp.base.L = L;

    auto_map_node_t* it = ev_map_find(&rt->schedule.all_table, &tmp.t_node);
    if (it == NULL)
    {
        return NULL;
    }

    atd_coroutine_impl_t* impl = container_of(it, atd_coroutine_impl_t, t_node);
    return &impl->base;
}

/**
 * @brief Register coroutine schedule hook.
 * @param[in] self  Managed coroutine context.
 * @param[in] fn    Schedule hook.
 * @param[in] arg   User defined argument passed to hook.
 * @return          Hook token.
 */
static auto_coroutine_hook_t* _coroutine_hook(struct auto_coroutine* self,
    auto_coroutine_hook_fn fn, void* arg)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    auto_coroutine_hook_t* token = malloc(sizeof(auto_coroutine_hook_t));
    token->fn = fn;
    token->arg = arg;
    token->impl = impl;
    ev_list_push_back(&impl->hook.queue, &token->node);

    return token;
}

/**
 * @brief Unregister coroutine schedule hook.
 * @param[in] self  Managed coroutine context.
 * @param[in] token Hook token returned by #_coroutine_hook().
 */
static void _coroutine_unhook(struct auto_coroutine* self, auto_coroutine_hook_t* token)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    if (impl->hook.it != NULL)
    {
        auto_coroutine_hook_t* next_hook = container_of(impl->hook.it, auto_coroutine_hook_t, node);

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

/**
 * @brief Set coroutine schedule state.
 * @param[in] self  Managed coroutine context.
 * @param[in] state Schedule state.
 */
static void _coroutine_set_state(struct auto_coroutine* self, int state)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    /* Backup and update state. */
    int old_state = impl->base.status;
    impl->base.status = state;

    /* move from wait_queue to busy_queue */
    if (!old_state && state)
    {
        ev_list_erase(&impl->rt->schedule.wait_queue, &impl->q_node);
        ev_list_push_back(&impl->rt->schedule.busy_queue, &impl->q_node);
        return;
    }

    /* move from busy_queue to wait_queue */
    if (old_state && !state)
    {
        ev_list_erase(&impl->rt->schedule.busy_queue, &impl->q_node);
        ev_list_push_back(&impl->rt->schedule.wait_queue, &impl->q_node);
        return;
    }
}

const auto_api_coroutine_t api_coroutine = {
    _coroutine_host,        /* .coroutine.host */
    _coroutine_find,        /* .coroutine.find */
    _coroutine_hook,        /* .coroutine.hook */
    _coroutine_unhook,      /* .coroutine.unhook */
    _coroutine_set_state,   /* .coroutine.set_state */
};
