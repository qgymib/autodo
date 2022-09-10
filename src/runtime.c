#include "runtime.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/**
 * @brief Global runtime.
 */
#define AUTO_GLOBAL "_AUTO_G"

static int _print_usage(lua_State* L, const char* name)
{
    const char* s_usage =
        "%s - A easy to use lua automation tool.\n"
        "Usage: %s [OPTIONS] [SCRIPT]\n"
        "  -c\n"
        "    Compile script.\n"
        "  -o\n"
        "    Output file path.\n"
        "  -h,--help\n"
        "    Show this help and exit.\n"
    ;
    return luaL_error(L, s_usage, name, name);
}

static int _init_parse_args_finalize(lua_State* L, atd_runtime_t* rt, const char* name)
{
    if (rt->config.script_path != NULL && rt->config.compile_path != NULL)
    {
        return luaL_error(L, "Conflict option: script followed by `-c`");
    }

    if (rt->config.script_path == NULL && rt->config.compile_path == NULL && rt->script.data == NULL)
    {
        return _print_usage(L, name);
    }

    if (rt->config.compile_path != NULL && rt->config.output_path == NULL)
    {
        const char* filename = get_filename(rt->config.compile_path);
        const char* ext = get_filename_ext(filename);

#if defined(_WIN32)
        size_t path_len = strlen(filename);
        size_t ext_len = strlen(ext);
        size_t malloc_size = path_len - ext_len + 4;
        rt->config.output_path = malloc(malloc_size);
        memcpy(rt->config.output_path, filename, path_len - ext_len);
        memcpy(rt->config.output_path + path_len - ext_len, "exe", 4);
#else
        size_t offset = ext - filename;
        rt->config.output_path = strdup(filename);
        rt->config.output_path[offset - 1] = '\0';
#endif
    }
    return 0;
}

static void _runtime_destroy_thread(atd_runtime_t* rt, lua_State* L, atd_thread_t* thr)
{
    assert(ev_list_size(&thr->hook.queue) == 0);

    ev_map_erase(&rt->schedule.all_table, &thr->t_node);

    if (thr->data.sch_status == LUA_TNONE)
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

static void _runtime_gc_release_coroutine(atd_runtime_t* rt, lua_State* L)
{
    ev_list_node_t* it;
    while ((it = ev_list_begin(&rt->schedule.busy_queue)) != NULL)
    {
        atd_thread_t* thr = container_of(it, atd_thread_t, q_node);
        _runtime_destroy_thread(rt, L, thr);
    }
    while ((it = ev_list_begin(&rt->schedule.wait_queue)) != NULL)
    {
        atd_thread_t* thr = container_of(it, atd_thread_t, q_node);
        _runtime_destroy_thread(rt, L, thr);
    }
}

static int _runtime_gc(lua_State* L)
{
    int ret;
    atd_runtime_t* rt = lua_touserdata(L, 1);
    rt = (atd_runtime_t*)ALIGN_WITH(rt, sizeof(void*) * 2);

    _runtime_gc_release_coroutine(rt, L);

    uv_close((uv_handle_t*)&rt->notifier, NULL);
    if ((ret = uv_loop_close(&rt->loop)) != 0)
    {
        return luaL_error(L, "close event loop failed: %d", ret);
    }

    if (rt->script.data != NULL)
    {
        free(rt->script.data);
        rt->script.data = NULL;
    }
    rt->script.size = 0;

    if (rt->config.compile_path != NULL)
    {
        free(rt->config.compile_path);
        rt->config.compile_path = NULL;
    }
    if (rt->config.output_path != NULL)
    {
        free(rt->config.output_path);
        rt->config.output_path = NULL;
    }
    if (rt->config.script_path != NULL)
    {
        free(rt->config.script_path);
        rt->config.script_path = NULL;
    }

    return 0;
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

static int _init_parse_args(lua_State* L, atd_runtime_t* rt, int argc, char* argv[])
{
    int i;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            return _print_usage(L, get_filename(argv[0]));
        }

        if (strcmp(argv[i], "-c") == 0)
        {
            i++;
            if (i >= argc)
            {
                return luaL_error(L, "missing argument to `-c`.");
            }

            if (rt->config.compile_path != NULL)
            {
                free(rt->config.compile_path);
            }
            rt->config.compile_path = atd_strdup(argv[i]);

            continue;
        }

        if (strcmp(argv[i], "-o") == 0)
        {
            i++;
            if (i >= argc)
            {
                return luaL_error(L, "missing argument to `-o`.");
            }

            if (rt->config.output_path != NULL)
            {
                free(rt->config.output_path);
            }
            rt->config.output_path = atd_strdup(argv[i]);

            continue;
        }

        if (rt->config.script_path != NULL)
        {
            free(rt->config.script_path);
        }
        rt->config.script_path = atd_strdup(argv[i]);
    }

    return _init_parse_args_finalize(L, rt, argv[0]);
}

static int _on_cmp_thread(const ev_map_node_t* key1, const ev_map_node_t* key2, void* arg)
{
    (void)arg;
    atd_thread_t* t1 = container_of(key1, atd_thread_t, t_node);
    atd_thread_t* t2 = container_of(key2, atd_thread_t, t_node);

    if (t1->co == t2->co)
    {
        return 0;
    }
    return t1->co < t2->co ? -1 : 1;
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
        _init_parse_args(L, rt, argc, argv);
    }
}

static void _thread_trigger_hook(atd_thread_t* thr)
{
    thr->hook.it = ev_list_begin(&thr->hook.queue);
    while (thr->hook.it != NULL)
    {
        atd_thread_hook_t* token = container_of(thr->hook.it, atd_thread_hook_t, node);
        thr->hook.it = ev_list_next(thr->hook.it);

        token->data.callback(token, thr, token->data.data);
    }
}

static int _runtime_schedule_one_pass(atd_runtime_t* rt, lua_State* L)
{
    ev_list_node_t* it = ev_list_begin(&rt->schedule.busy_queue);
    while (it != NULL)
    {
        atd_thread_t* thr = container_of(it, atd_thread_t, q_node);
        it = ev_list_next(it);

        /* Resume coroutine */
        int ret = lua_resume(thr->co, L, 0, &thr->data.n_ret);

        /* Coroutine yield */
        if (ret == LUA_YIELD)
        {
            _thread_trigger_hook(thr);

            /* Anything received treat as busy coroutine */
            if (thr->data.n_ret != 0)
            {
                lua_pop(thr->co, thr->data.n_ret);
            }

            continue;
        }

        /* Coroutine either finish execution or error happen.  */
        atd_set_thread_state(rt, thr, ret);
        /* Call hook */
        _thread_trigger_hook(thr);
        /* Destroy coroutine */
        _runtime_destroy_thread(rt, L, thr);
    }

    return 0;
}

int atd_init_runtime(lua_State* L, int argc, char* argv[])
{
    /*
     * We cannot trust LUA memory allocation, it does not align it to twice of
     * machine size. In most case it is fine, but not for #atd_runtime_t.
     *
     * In Windows, the jmp_buf is force align to 16 bytes. If the address is
     * not aligned to 16, the setjmp() will trigger coredump.
     *
     * To fix it, we allocate 16 more bytes and manually align to 16 bytes.
     */
    size_t malloc_size = sizeof(atd_runtime_t) + sizeof(void*) * 2;
    atd_runtime_t* rt = lua_newuserdata(L, malloc_size);
    memset(rt, 0, malloc_size);
    /* Align to 16 bytes */
    rt = (atd_runtime_t*)ALIGN_WITH(rt, sizeof(void*) * 2);

    static const luaL_Reg s_runtime_meta[] = {
            { "__gc",   _runtime_gc },
            { NULL,     NULL },
    };
    if (luaL_newmetatable(L, "__auto_runtime") != 0)
    {
        luaL_setfuncs(L, s_runtime_meta, 0);
    }
    lua_setmetatable(L, -2);

    _init_runtime(L, rt, argc, argv);
    lua_setglobal(L, AUTO_GLOBAL);

    return 0;
}

atd_runtime_t* atd_get_runtime(lua_State* L)
{
    int sp = lua_gettop(L);

    if (lua_getglobal(L, AUTO_GLOBAL) != LUA_TUSERDATA)
    {
        luaL_error(L, "missing global runtime `%s`", AUTO_GLOBAL);
        return NULL;
    }

    atd_runtime_t* rt = lua_touserdata(L, sp + 1);
    rt = (atd_runtime_t*)ALIGN_WITH(rt, sizeof(void*) * 2);

    lua_settop(L, sp);

    return rt;
}

atd_thread_t* atd_new_thread(atd_runtime_t* rt, lua_State* L)
{
    atd_thread_t* thr = malloc(sizeof(atd_thread_t));

    thr->co = lua_newthread(L);
    thr->data.n_ret = 0;
    thr->data.sch_status = LUA_TNONE;
    thr->data.ref_key = luaL_ref(L, LUA_REGISTRYINDEX);
    thr->hook.it = NULL;
    ev_list_init(&thr->hook.queue);

    ev_list_push_back(&rt->schedule.busy_queue, &thr->q_node);
    ev_map_insert(&rt->schedule.all_table, &thr->t_node);

    return thr;
}

atd_thread_t* atd_find_thread(atd_runtime_t* rt, lua_State* L)
{
    atd_thread_t tmp;
    tmp.co = L;

    ev_map_node_t* it = ev_map_find(&rt->schedule.all_table, &tmp.t_node);
    if (it == NULL)
    {
        return NULL;
    }

    return container_of(it, atd_thread_t, t_node);
}

void atd_set_thread_state(atd_runtime_t* rt, atd_thread_t* thr, int state)
{
    /* In busy_queue */
    if (thr->data.sch_status == LUA_TNONE)
    {
        if (state == LUA_TNONE)
        {/* Do nothing if new state is also BUSY */
            return;
        }

        ev_list_erase(&rt->schedule.busy_queue, &thr->q_node);
        thr->data.sch_status = state;
        ev_list_push_back(&rt->schedule.wait_queue, &thr->q_node);
        return;
    }

    /* We are in wait_queue, cannot operate on dead coroutine */
    if (thr->data.sch_status != LUA_YIELD)
    {
        abort();
    }

    /* move to busy_queue */
    if (state == LUA_TNONE)
    {
        ev_list_erase(&rt->schedule.wait_queue, &thr->q_node);
        thr->data.sch_status = state;
        ev_list_push_back(&rt->schedule.busy_queue, &thr->q_node);
    }

    /* thr is dead, keep in wait_queue */
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

int atd_runtime_link(lua_State* L, int idx)
{
    if (lua_getglobal(L, AUTO_GLOBAL) != LUA_TUSERDATA)
    {
        return luaL_error(L, "missing global runtime `%s`", AUTO_GLOBAL);
    }

    lua_setuservalue(L, idx);
    return 0;
}

void atd_hook_thread(atd_thread_hook_t* token, atd_thread_t* thr,
                     atd_thread_hook_fn fn, void* arg)
{
    token->data.callback = fn;
    token->data.data = arg;
    token->data.thr = thr;
    ev_list_push_back(&thr->hook.queue, &token->node);
}

void atd_unhook_thread(atd_thread_hook_t* token)
{
    atd_thread_t* thr = token->data.thr;

    if (thr->hook.it != NULL)
    {
        atd_thread_hook_t* next_hook = container_of(thr->hook.it, atd_thread_hook_t, node);

        /* If this is next hook, move iterator to next node. */
        if (next_hook == token)
        {
            thr->hook.it = ev_list_next(thr->hook.it);
        }
    }

    ev_list_erase(&thr->hook.queue, &token->node);
    token->data.thr = NULL;
}
