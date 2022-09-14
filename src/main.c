#include "runtime.h"
#include "utils.h"
#include "lua/api.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

/**
 * @brief Initialize lua runtime.
 * @note Use lua_pcall().
 * @param[in] L     Lua VM.
 * @return          true to continue, false to stop.
 */
static void _init_lua_runtime(int argc, char* argv[])
{
    atd_init_runtime(argc, argv);

    luaL_openlibs(atd_rt->L);
    auto_init_libs(atd_rt->L);
}

static int _user_thread_on_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)L; (void)status; (void)ctx;
    // nothing to do
    return 0;
}

static int _user_thread(lua_State* L)
{
    if (luaL_loadbuffer(L, atd_rt->script.data, atd_rt->script.size,
        atd_rt->config.script_name) != LUA_OK)
    {
        return lua_error(L);
    }

    lua_callk(L, 0, LUA_MULTRET, 0, _user_thread_on_resume);
    return 0;
}

static int _run_script(lua_State* L, atd_runtime_t* rt)
{
    atd_coroutine_t* thr = api.register_coroutine(lua_newthread(L));
    lua_pop(L, 1);

    lua_pushcfunction(thr->L, _user_thread);

    return atd_schedule(rt, L);
}

static int _lua_load_script(atd_runtime_t* rt, lua_State* L)
{
    if (rt->script.data != NULL)
    {
        free(rt->script.data);
    }

    if (rt->config.script_name != NULL)
    {
        free(rt->config.script_name);
    }
    rt->config.script_name = atd_strdup(get_filename(rt->config.script_path));

    int ret = atd_readfile(rt->config.script_path,
        &rt->script.data, &rt->script.size);
    if (ret == 0)
    {
        return 0;
    }

    return luaL_error(L, "open `%s` failed: %s(%d)",
        rt->config.script_path,
        atd_strerror(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)),
        ret);
}

static int _lua_run(lua_State* L)
{
    /* Load script if necessary */
    if (atd_rt->config.script_path != NULL)
    {
        _lua_load_script(atd_rt, L);
    }

    /* Run script */
    if (atd_rt->script.data != NULL)
    {
        return _run_script(L, atd_rt);
    }

    return luaL_error(L, "no operation");
}

int main(int argc, char* argv[])
{
    atexit(atd_exit_runtime);

    _init_lua_runtime(argc, argv);

    if (setjmp(atd_rt->check.point) != 0)
    {
        goto vm_exit;
    }

    lua_pushcfunction(atd_rt->L, _lua_run);
    int ret = lua_pcall(atd_rt->L, 0, 0, 0);
    if (ret != LUA_OK)
    {
        fprintf(stderr, "%s\n", lua_tostring(atd_rt->L, -1));
    }

vm_exit:
    return ret;
}
