#include "runtime.h"
#include "api/coroutine.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/**
 * @brief Global runtime.
 */
#define AUTO_GLOBAL "_AUTO_G"

static void _print_usage(const char* name)
{
    const char* s_usage =
        "%s - A easy to use lua automation tool.\n"
        "Usage: %s [OPTIONS] [SCRIPT]\n"
        "  -h,--help\n"
        "    Show this help and exit.\n"
    ;
    fprintf(stdout, s_usage, name, name);
    exit(EXIT_SUCCESS);
}

static int _init_parse_args_finalize(atd_runtime_t* rt, const char* name)
{
    if (rt->config.script_file == NULL && rt->script.data == NULL)
    {
        _print_usage(name);
    }

    return 0;
}

static void _runtime_destroy_thread(atd_runtime_t* rt, lua_State* L, atd_coroutine_impl_t* thr)
{
    assert(ev_list_size(&thr->hook.queue) == 0);

    ev_map_erase(&rt->schedule.all_table, &thr->t_node);

    if (thr->base.status == LUA_TNONE)
    {
        ev_list_erase(&rt->schedule.busy_queue, &thr->q_node);
    }
    else
    {
        ev_list_erase(&rt->schedule.wait_queue, &thr->q_node);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, thr->data.ref_key);
    thr->data.ref_key = LUA_NOREF;
    free(thr);
}

static void _runtime_gc_release_coroutine(atd_runtime_t* rt)
{
    auto_list_node_t * it;
    while ((it = ev_list_begin(&rt->schedule.busy_queue)) != NULL)
    {
        atd_coroutine_impl_t* thr = container_of(it, atd_coroutine_impl_t, q_node);
        _runtime_destroy_thread(rt, rt->L, thr);
    }
    while ((it = ev_list_begin(&rt->schedule.wait_queue)) != NULL)
    {
        atd_coroutine_impl_t* thr = container_of(it, atd_coroutine_impl_t, q_node);
        _runtime_destroy_thread(rt, rt->L, thr);
    }
}

static int _init_parse_args(atd_runtime_t* rt, int argc, char* argv[])
{
    int i;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            _print_usage(get_filename(argv[0]));
        }

        if (rt->config.script_file != NULL)
        {
            free(rt->config.script_file);
        }
        rt->config.script_file = atd_strdup(argv[i]);
    }

    return _init_parse_args_finalize(rt, argv[0]);
}

static int _on_cmp_thread(const auto_map_node_t* key1, const auto_map_node_t* key2, void* arg)
{
    (void)arg;
    atd_coroutine_impl_t* t1 = container_of(key1, atd_coroutine_impl_t, t_node);
    atd_coroutine_impl_t* t2 = container_of(key2, atd_coroutine_impl_t, t_node);

    if (t1->base.L == t2->base.L)
    {
        return 0;
    }
    return t1->base.L < t2->base.L ? -1 : 1;
}

static void _on_runtime_notify(uv_async_t *handle)
{
    (void)handle;
}

static void _thread_trigger_hook(atd_coroutine_impl_t* thr)
{
    thr->hook.it = ev_list_begin(&thr->hook.queue);
    while (thr->hook.it != NULL)
    {
        auto_coroutine_hook_t* token = container_of(thr->hook.it, auto_coroutine_hook_t, node);
        thr->hook.it = ev_list_next(thr->hook.it);

        token->fn(&token->impl->base, token->arg);
    }
}

static int _runtime_schedule_one_pass(atd_runtime_t* rt, lua_State* L)
{
    auto_list_node_t * it = ev_list_begin(&rt->schedule.busy_queue);
    while (it != NULL)
    {
        atd_coroutine_impl_t* thr = container_of(it, atd_coroutine_impl_t, q_node);
        it = ev_list_next(it);

        /* Resume coroutine */
        int ret = lua_resume(thr->base.L, L, thr->base.nresults, &thr->base.nresults);

        /* Coroutine yield */
        if (ret == LUA_YIELD)
        {
            _thread_trigger_hook(thr);

            /* Anything received treat as busy coroutine */
            if (thr->base.nresults != 0)
            {
                lua_pop(thr->base.L, thr->base.nresults);
                thr->base.nresults = 0;
            }

            continue;
        }

        /* Coroutine either finish execution or error happen.  */
        api_coroutine.set_state(&thr->base, ret);
        /* Call hook */
        _thread_trigger_hook(thr);

        /* Error affect main thread */
        if (ret != LUA_OK && !thr->flags.protect)
        {
            lua_xmove(thr->base.L, L, 1);
            return lua_error(L);
        }

        /* Destroy coroutine */
        _runtime_destroy_thread(rt, L, thr);
    }

    return 0;
}

static void _init_runtime(atd_runtime_t* rt, int argc, char* argv[])
{
    uv_loop_init(&rt->loop);
    uv_async_init(&rt->loop, &rt->notifier, _on_runtime_notify);

    ev_list_init(&rt->schedule.busy_queue);
    ev_list_init(&rt->schedule.wait_queue);
    ev_map_init(&rt->schedule.all_table, _on_cmp_thread, NULL);

    int ret;
    if ((ret = atd_read_self_script(&rt->script.data, &rt->script.size)) != 0)
    {
        fprintf(stderr, "read self failed: %s(%d)\n",
                atd_strerror(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)), ret);
        exit(EXIT_FAILURE);
    }

    /* Command line argument is only parsed when script is not embed */
    if (rt->script.data == NULL)
    {
        _init_parse_args(rt, argc, argv);
    }
}

static void _runtime_exit(atd_runtime_t* rt)
{
    int ret;

    /* Release all coroutine */
    _runtime_gc_release_coroutine(rt);

    /* Close all handles */
    uv_close((uv_handle_t*)&rt->notifier, NULL);
    uv_run(&rt->loop, UV_RUN_DEFAULT);

    if ((ret = uv_loop_close(&rt->loop)) != 0)
    {
        fprintf(stderr, "close event loop failed:%s:%d\n",
                uv_strerror_r(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)), ret);
        uv_print_all_handles(&rt->loop, stderr);
        abort();
    }

    if (rt->script.data != NULL)
    {
        free(rt->script.data);
        rt->script.data = NULL;
    }
    rt->script.size = 0;

    if (rt->config.script_file != NULL)
    {
        free(rt->config.script_file);
        rt->config.script_file = NULL;
    }
    if (rt->config.script_path != NULL)
    {
        free(rt->config.script_path);
        rt->config.script_path = NULL;
    }
    if (rt->config.script_name != NULL)
    {
        free(rt->config.script_name);
        rt->config.script_name = NULL;
    }
}

static int _auto_runtime_gc(lua_State* L)
{
    atd_runtime_t* rt = lua_touserdata(L, 1);
    _runtime_exit(rt);
    return 0;
}

int atd_init_runtime(lua_State* L, int argc, char* argv[])
{
    atd_runtime_t* rt = lua_newuserdata(L, sizeof(atd_runtime_t));

    memset(rt, 0, sizeof(atd_runtime_t));
    rt->L = L;

    static const luaL_Reg s_auto_meta[] = {
        { "__gc", _auto_runtime_gc },
        { NULL, NULL },
    };
    if (luaL_newmetatable(L, AUTO_GLOBAL) != 0)
    {
        luaL_setfuncs(L, s_auto_meta, 0);
    }
    lua_setmetatable(L, -2);

    lua_setglobal(L, AUTO_GLOBAL);

    _init_runtime(rt, argc, argv);

    return 0;
}

atd_runtime_t* auto_get_runtime(lua_State* L)
{
    lua_getglobal(L, AUTO_GLOBAL);
    atd_runtime_t* rt = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return rt;
}

int auto_schedule(atd_runtime_t* rt, lua_State* L)
{
    for (;;)
    {
        _runtime_schedule_one_pass(rt, L);

        if (ev_map_size(&rt->schedule.all_table) == 0)
        {
            break;
        }

        uv_run_mode mode = UV_RUN_ONCE;
        if (ev_list_size(&rt->schedule.busy_queue) != 0)
        {
            mode = UV_RUN_NOWAIT;
        }

        uv_run(&rt->loop, mode);
    }

    return 0;
}
