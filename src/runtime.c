#include "runtime.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/**
 * @brief Global runtime.
 */
#define AUTO_GLOBAL "_AUTO_G"

atd_runtime_t*   g_rt;

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
    if (g_rt->config.script_file == NULL && g_rt->script.data == NULL)
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
    atd_list_node_t * it;
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

static int _init_parse_args(int argc, char* argv[])
{
    int i;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            _print_usage(get_filename(argv[0]));
        }

        if (g_rt->config.script_file != NULL)
        {
            free(g_rt->config.script_file);
        }
        g_rt->config.script_file = atd_strdup(argv[i]);
    }

    return _init_parse_args_finalize(argv[0]);
}

static int _on_cmp_thread(const atd_map_node_t* key1, const atd_map_node_t* key2, void* arg)
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
        atd_coroutine_hook_t* token = container_of(thr->hook.it, atd_coroutine_hook_t, node);
        thr->hook.it = ev_list_next(thr->hook.it);

        token->fn(&token->impl->base, token->arg);
    }
}

static int _runtime_schedule_one_pass(atd_runtime_t* rt, lua_State* L)
{
    atd_list_node_t * it = ev_list_begin(&rt->schedule.busy_queue);
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
        api.coroutine.set_state(&thr->base, ret);
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

static void _init_runtime(int argc, char* argv[])
{
    g_rt->L = luaL_newstate();;

    uv_loop_init(&g_rt->loop);
    uv_async_init(&g_rt->loop, &g_rt->notifier, _on_runtime_notify);

    ev_list_init(&g_rt->schedule.busy_queue);
    ev_list_init(&g_rt->schedule.wait_queue);
    ev_map_init(&g_rt->schedule.all_table, _on_cmp_thread, NULL);

    int ret;
    if ((ret = atd_read_self_script(&g_rt->script.data, &g_rt->script.size)) != 0)
    {
        fprintf(stderr, "read self failed: %s(%d)\n",
                atd_strerror(ret, g_rt->cache.errbuf, sizeof(g_rt->cache.errbuf)), ret);
        exit(EXIT_FAILURE);
    }

    /* Command line argument is only parsed when script is not embed */
    if (g_rt->script.data == NULL)
    {
        _init_parse_args(argc, argv);
    }
}

int atd_init_runtime(int argc, char* argv[])
{
    uv_setup_args(argc, argv);
    uv_disable_stdio_inheritance();

    g_rt = malloc(sizeof(atd_runtime_t));
    memset(g_rt, 0, sizeof(*g_rt));

    _init_runtime(argc, argv);

    return 0;
}

void atd_exit_runtime(void)
{
    int ret;

    /* Release all coroutine */
    _runtime_gc_release_coroutine(g_rt);

    lua_close(g_rt->L);
    g_rt->L = NULL;

    /* Close all handles */
    uv_close((uv_handle_t*)&g_rt->notifier, NULL);
    uv_run(&g_rt->loop, UV_RUN_DEFAULT);

    if ((ret = uv_loop_close(&g_rt->loop)) != 0)
    {
        fprintf(stderr, "close event loop failed:%s:%d\n",
                uv_strerror_r(ret, g_rt->cache.errbuf, sizeof(g_rt->cache.errbuf)), ret);
        uv_print_all_handles(&g_rt->loop, stderr);
        abort();
    }

    if (g_rt->script.data != NULL)
    {
        free(g_rt->script.data);
        g_rt->script.data = NULL;
    }
    g_rt->script.size = 0;

    if (g_rt->config.script_file != NULL)
    {
        free(g_rt->config.script_file);
        g_rt->config.script_file = NULL;
    }
    if (g_rt->config.script_path != NULL)
    {
        free(g_rt->config.script_path);
        g_rt->config.script_path = NULL;
    }
    if (g_rt->config.script_name != NULL)
    {
        free(g_rt->config.script_name);
        g_rt->config.script_name = NULL;
    }

    free(g_rt);
    g_rt = NULL;

    uv_library_shutdown();
}

int atd_schedule(atd_runtime_t* rt, lua_State* L)
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
