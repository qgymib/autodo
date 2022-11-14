#include "runtime.h"
#include "package.h"
#include "utils.h"
#include "lua/api.h"
#include "api/coroutine.h"
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
static void _init_lua_runtime(lua_State* L, int argc, char* argv[])
{
    luaL_openlibs(L);

    atd_init_runtime(L, argc, argv);
    auto_init_libs(L);
}

static int _user_thread_on_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)L; (void)status; (void)ctx;
    // nothing to do
    return 0;
}

static int _user_thread(lua_State* L)
{
    auto_runtime_t* rt = auto_get_runtime(L);

    if (luaL_loadbuffer(L, rt->script.data, rt->script.size,
        rt->config.script_name) != LUA_OK)
    {
        return lua_error(L);
    }

    lua_callk(L, 0, LUA_MULTRET, 0, _user_thread_on_resume);
    return 0;
}

static int _run_script(lua_State* L, auto_runtime_t* rt)
{
    auto_coroutine_t* thr = api_coroutine.host(lua_newthread(L));
    lua_pop(L, 1);

    lua_pushcfunction(thr->L, _user_thread);

    return auto_schedule(rt, L);
}

static int _lua_load_script(auto_runtime_t* rt, lua_State* L)
{
    const char* filename = get_filename(rt->config.script_file);

    if (rt->config.script_name != NULL)
    {
        free(rt->config.script_name);
    }
    rt->config.script_name = auto_strdup(filename);

    if (rt->config.script_path != NULL)
    {
        free(rt->config.script_path);
    }
    size_t malloc_size = filename - rt->config.script_file;
    rt->config.script_path = malloc(malloc_size);
    memcpy(rt->config.script_path, rt->config.script_file, malloc_size);
    rt->config.script_path[malloc_size - 1] = '\0';

    if (rt->script.data != NULL)
    {
        free(rt->script.data);
    }

    int ret = atd_readfile(rt->config.script_file,
        &rt->script.data, &rt->script.size);
    if (ret == 0)
    {
        return 0;
    }

    return luaL_error(L, "open `%s` failed: %s(%d)",
        rt->config.script_file,
        auto_strerror(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)),
        ret);
}

static void _inject_require_searcher(lua_State* L)
{
    int ret;
    int sp = lua_gettop(L);

    /* sp + 1 */
    if ((ret = lua_getglobal(L, "package")) != LUA_TTABLE)
    {
        abort();
    }
    /* sp + 2 */
    if ((ret = lua_getfield(L, sp + 1, "searchers")) != LUA_TTABLE)
    {
        abort();
    }

    /* Append custom loader to the end of searchers table */
    lua_pushcfunction(L, atd_package_loader);
    size_t len = luaL_len(L, sp + 2);
    lua_seti(L, sp + 2, len + 1);

    /* Resource stack */
    lua_settop(L, sp);
}

static int _lua_run(lua_State* L)
{
    auto_runtime_t* rt = auto_get_runtime(L);

    _inject_require_searcher(L);

    /* Load script if necessary */
    if (rt->config.script_file != NULL)
    {
        _lua_load_script(rt, L);
    }

    /* Run script */
    if (rt->script.data != NULL)
    {
        return _run_script(L, rt);
    }

    return luaL_error(L, "no operation");
}

int main(int argc, char* argv[])
{
    argv = uv_setup_args(argc, argv);
    uv_disable_stdio_inheritance();

    lua_State* L = luaL_newstate();
    _init_lua_runtime(L, argc, argv);

    lua_pushcfunction(L, _lua_run);
    int ret = lua_pcall(L, 0, 0, 0);
    if (ret != LUA_OK)
    {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
    }

    lua_close(L);
    uv_library_shutdown();

    return ret;
}
