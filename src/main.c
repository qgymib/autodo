#include "runtime.h"
#include "utils.h"
#include "lua/api.h"
#include "gui/gui.h"
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
static int _init_lua_runtime(lua_State* L)
{
    luaL_openlibs(L);
    auto_init_libs(L);

    int argc = (int)lua_tointeger(L, 1);
    char** argv = (char**)lua_touserdata(L, 2);
    return auto_init_runtime(L, argc, argv);
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

    if (luaL_loadbuffer(L, rt->script.data, rt->script.size, "script") != LUA_OK)
    {
        return lua_error(L);
    }

    lua_callk(L, 0, LUA_MULTRET, 0, _user_thread_on_resume);
    return 0;
}

static int _run_script(lua_State* L, auto_runtime_t* rt)
{
    auto_thread_t* u_thread = auto_new_thread(rt, L);
    lua_pushcfunction(u_thread->co, _user_thread);

    return auto_schedule(rt, L);
}

static int _lua_load_script(auto_runtime_t* rt, lua_State* L)
{
    if (rt->script.data != NULL)
    {
        free(rt->script.data);
    }

    int ret = auto_readfile(rt->config.script_path,
        &rt->script.data, &rt->script.size);
    if (ret == 0)
    {
        return 0;
    }

    return luaL_error(L, "open `%s` failed: %s(%d)",
        rt->config.script_path,
        auto_strerror(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)),
        ret);
}

static int _lua_run(lua_State* L)
{
    auto_runtime_t* rt = auto_get_runtime(L);

    /* Load script if necessary */
    if (rt->config.script_path != NULL)
    {
        _lua_load_script(rt, L);
    }

    /* Run script */
    if (rt->script.data != NULL)
    {
        return _run_script(L, rt);
    }

    if (rt->config.compile_path != NULL)
    {
        return auto_compile_script(L, rt->config.compile_path, rt->config.output_path);
    }

    return luaL_error(L, "no operation");
}

static void _control_routine(void* data)
{
    lua_State* L = data;
    auto_runtime_t* rt = auto_get_runtime(L);

    if (setjmp(rt->check.point) != 0)
    {
        goto vm_exit;
    }

    while (!rt->flag.gui_ready)
    {
        uv_sleep(10);
    }

    lua_pushcfunction(L, _lua_run);
    int ret = lua_pcall(L, 0, 0, 0);
    if (ret != LUA_OK)
    {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
    }

vm_exit:
    /* Ask GUI to exit */
    auto_gui_exit();
}

static void _on_gui_event(auto_gui_msg_t* msg, void* udata)
{
    auto_runtime_t* rt = udata;

    switch (msg->type)
    {
    case AUTO_GUI_READY:
        rt->flag.gui_ready = 1;
        break;

    case AUTO_GUI_QUIT:
        rt->flag.looping = 0;
        uv_async_send(&rt->notifier);
        break;

    default:
        break;
    }
}

static void _setup(lua_State* L, int argc, char* argv[])
{
    uv_setup_args(argc, argv);

    /* Initialize Lua VM */
    lua_pushcfunction(L, _init_lua_runtime);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);

    /* Initialize failure */
    if (lua_pcall(L, 2, 1, 0) == LUA_OK)
    {
        return;
    }

    /*
     * Let's do some dirty hack to check if it is a help string.
     */
    int exit_ret = EXIT_FAILURE;
    const char* err_msg = lua_tostring(L, -1);
    if (strstr(err_msg, "Usage:") != NULL)
    {
        exit_ret = EXIT_SUCCESS;
        fprintf(stdout, "%s\n", err_msg);
    }
    else
    {
        fprintf(stderr, "%s\n", err_msg);
    }

    lua_close(L);
    exit(exit_ret);
}

#if defined(_WIN32)
int WINAPI WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow)
#else
int main(int argc, char* argv[])
#endif
{
    auto_gui_startup_info_t info;
    memset(&info, 0, sizeof(info));

#if defined(_WIN32)
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine;
    info.argc = __argc;
    info.argv = __argv;
    info.nShowCmd = nCmdShow;
#else
    info.argc = argc;
    info.argv = argv;
#endif

    lua_State* L = luaL_newstate();
    _setup(L, info.argc, info.argv);

    info.udata = auto_get_runtime(L);
    info.on_event = _on_gui_event;

    uv_thread_t tid;
    uv_thread_create(&tid, _control_routine, L);

    /* Initialize GUI */
    int ret = auto_gui(&info);

    /* Cleanup */
    uv_thread_join(&tid);
    lua_close(L);

    return ret;
}
