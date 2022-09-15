#include "runtime.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/**
 * @brief Global runtime.
 */
#define AUTO_GLOBAL "_AUTO_G"

atd_runtime_t*   atd_rt;

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

static int _init_parse_args_finalize(const char* name)
{
    if (atd_rt->config.script_path == NULL && atd_rt->script.data == NULL)
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
    ev_list_node_t* it;
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

void atd_exit_runtime(void)
{
    int ret;

    _runtime_gc_release_coroutine(atd_rt);

    uv_close((uv_handle_t*)&atd_rt->notifier, NULL);

    /* Close all handles */
    uv_run(&atd_rt->loop, UV_RUN_DEFAULT);

    if ((ret = uv_loop_close(&atd_rt->loop)) != 0)
    {
        luaL_error(atd_rt->L, "close event loop failed: %d", ret);
    }

    if (atd_rt->script.data != NULL)
    {
        free(atd_rt->script.data);
        atd_rt->script.data = NULL;
    }
    atd_rt->script.size = 0;

    if (atd_rt->config.script_path != NULL)
    {
        free(atd_rt->config.script_path);
        atd_rt->config.script_path = NULL;
    }
    if (atd_rt->config.script_name != NULL)
    {
        free(atd_rt->config.script_name);
        atd_rt->config.script_name = NULL;
    }

    lua_close(atd_rt->L);
    free(atd_rt);
    atd_rt = NULL;

    uv_library_shutdown();
}

static int _init_runtime_script(lua_State* L, atd_runtime_t* rt)
{
    int ret;

    if ((ret = atd_read_self_script(&rt->script.data, &rt->script.size)) != 0)
    {
        return luaL_error(L, "read self failed: %s(%d)",
            atd_strerror(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)), ret);
    }

    return 0;
}

static int _init_parse_args(int argc, char* argv[])
{
    int i;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            _print_usage(get_filename(argv[0]));
        }

        if (atd_rt->config.script_path != NULL)
        {
            free(atd_rt->config.script_path);
        }
        atd_rt->config.script_path = atd_strdup(argv[i]);
    }

    return _init_parse_args_finalize(argv[0]);
}

static int _on_cmp_thread(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
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

static void _init_runtime(lua_State* L, atd_runtime_t* rt, int argc, char* argv[])
{
    uv_loop_init(&rt->loop);
    uv_async_init(&rt->loop, &rt->notifier, _on_runtime_notify);

    rt->flag.looping = 1;
    ev_list_init(&rt->schedule.busy_queue);
    ev_list_init(&rt->schedule.wait_queue);
    ev_map_init(&rt->schedule.all_table, _on_cmp_thread, NULL);

    _init_runtime_script(L, rt);

    /* Command line argument is only parsed when script is not embed */
    if (rt->script.data == NULL)
    {
        _init_parse_args(argc, argv);
    }
}

static void _thread_trigger_hook(atd_coroutine_impl_t* thr)
{
    thr->hook.it = ev_list_begin(&thr->hook.queue);
    while (thr->hook.it != NULL)
    {
        atd_coroutine_hook_t* token = container_of(thr->hook.it, atd_coroutine_hook_t, node);
        thr->hook.it = ev_list_next(thr->hook.it);

        token->fn(&token->impl->base, token->arg);
    }
}

static int _runtime_schedule_one_pass(atd_runtime_t* rt, lua_State* L)
{
    ev_list_node_t* it = ev_list_begin(&rt->schedule.busy_queue);
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
        thr->base.set_schedule_state(&thr->base, ret);
        /* Call hook */
        _thread_trigger_hook(thr);

        /* Error affect main thread */
        if (ret != LUA_OK)
        {
            lua_xmove(thr->base.L, L, 1);
            return lua_error(L);
        }

        /* Destroy coroutine */
        _runtime_destroy_thread(rt, L, thr);
    }

    return 0;
}

int atd_init_runtime(int argc, char* argv[])
{
    uv_setup_args(argc, argv);
    uv_disable_stdio_inheritance();

    atd_rt = malloc(sizeof(atd_runtime_t));
    memset(atd_rt, 0, sizeof(*atd_rt));

    atd_rt->L = luaL_newstate();;
    _init_runtime(atd_rt->L, atd_rt, argc, argv);

    return 0;
}

int atd_schedule(atd_runtime_t* rt, lua_State* L)
{
    while(rt->flag.looping)
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
