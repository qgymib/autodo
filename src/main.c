#include "runtime.h"
#include "utils.h"
#include "require/loader.h"
#include "lua/api.h"
#include "lua/coroutine.h"
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
    /* Initialize lua standard library */
    luaL_openlibs(L);

    /* Initialize runtime */
    auto_init_runtime(L);

    /* Initialize interface for lua */
    auto_init_libs(L);

    /* Custom runtime by command line arguments */
    auto_custom_runtime(L, argc, argv);
}

static int _user_thread_on_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)L; (void)status; (void)ctx;
    // nothing to do
    return 0;
}

static int _user_thread(lua_State* L)
{
    atd_runtime_t* rt = auto_get_runtime(L);

    if (luaL_loadbuffer(L, rt->script.data, rt->script.size,
        rt->config.script_name) != LUA_OK)
    {
        return lua_error(L);
    }

    lua_callk(L, 0, LUA_MULTRET, 0, _user_thread_on_resume);
    return 0;
}

static int _run_script(lua_State* L, atd_runtime_t* rt)
{
    atd_coroutine_t* thr = api_coroutine.host(lua_newthread(L));
    lua_pop(L, 1);

    lua_pushcfunction(thr->L, _user_thread);

    return atd_schedule(rt, L);
}

static int _lua_load_script(atd_runtime_t* rt, lua_State* L)
{
    const char* filename = get_filename(rt->config.script_file);

    if (rt->config.script_name != NULL)
    {
        free(rt->config.script_name);
    }
    rt->config.script_name = atd_strdup(filename);

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
        atd_strerror(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)),
        ret);
}

static int _lua_run(lua_State* L)
{
    atd_runtime_t* rt = auto_get_runtime(L);

    auto_inject_loader(L);

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
