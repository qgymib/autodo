#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include "lua/api.h"
#include "lua/sleep.h"
#include "gui/gui.h"
#include "runtime.h"
#include "utils.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

static int _write_executable(lua_State* L, const char* dst)
{
    FILE* dst_file;
    int errcode;
    char errbuf[1024];

#if defined(_WIN32)
    errcode = fopen_s(&dst_file, dst, "wb");
#else
    dst_file = fopen(dst, "wb");
    errcode = errno;
#endif

    (void)errcode;
    if (dst_file == NULL)
    {
        return luaL_error(L, "open `%s` failed: %s(%d).", dst,
            auto_strerror(errno, errbuf, sizeof(errbuf)), errno);
    }

    {
        void* exe_data; size_t exe_size;
        auto_read_self(&exe_data, &exe_size);
        fwrite(exe_data, exe_size, 1, dst_file);
    }

    {
        auto_probe_t probe;
        _init_probe(&probe);
        fwrite(&probe, sizeof(probe), 1, dst_file);
    }

    {
        size_t size;
        const char* data = lua_tolstring(L, -1, &size);
        fwrite(data, size, 1, dst_file);
    }

    fclose(dst_file);

#if !defined(_WIN32)
    chmod(dst, 0777);
#endif

    return 0;
}

static int _compile_script(lua_State* L, const char* src, const char* dst)
{
    char errbuf[1024];

    int ret = luaL_loadfile(L, src);
    if (ret == LUA_ERRFILE)
    {
        return luaL_error(L, "open `%s` failed.", src);
    }
    if (ret != LUA_OK)
    {
        return lua_error(L);
    }

    void* data;
    size_t size;
    ret = auto_readfile(src, &data, &size);
    if (ret != 0)
    {
        return luaL_error(L, "open `%s` failed: %s(%d)",
            src, auto_strerror(ret, errbuf, sizeof(errbuf)), ret);
    }

    lua_pushlstring(L, data, size);
    free(data);

    return _write_executable(L, dst);
}

static int _run_script(lua_State* L, const char* path)
{
    int ret = luaL_dofile(L, path);
    if (ret == LUA_ERRFILE)
    {
        return luaL_error(L, "open `%s` failed.", path);
    }
    if (ret == LUA_OK)
    {
        return 0;
    }
    return lua_error(L);
}

/**
 * @brief Initialize lua runtime.
 * @note Use lua_pcall().
 * @param[in] L     Lua VM.
 * @return          true to continue, false to stop.
 */
static int _lua_init_vm(lua_State* L)
{
    /* Initialize library */
    luaL_openlibs(L);
    auto_init_libs(L);

    /* table.runtime */
    int argc = lua_tointeger(L, 1);
    char** argv = lua_touserdata(L, 2);
    return auto_init_runtime(L, argc, argv);
}

static int _lua_run(lua_State* L)
{
    auto_runtime_t* rt = auto_get_runtime(L);

    if (rt->script.data != NULL)
    {
        if (luaL_loadbuffer(L, rt->script.data, rt->script.size, "script") != LUA_OK)
        {
            return lua_error(L);
        }
        lua_call(L, 0, LUA_MULTRET);
        return 0;
    }

    if (rt->config.script_path != NULL)
    {
        return _run_script(L, rt->config.script_path);
    }

    if (rt->config.compile_path != NULL)
    {
        return _compile_script(L, rt->config.compile_path, rt->config.output_path);
    }

    return luaL_error(L, "no operation");
}

static void* _lua_routine(void* data)
{
    lua_State* L = data;
    auto_runtime_t* rt = auto_get_runtime(L);

    if (setjmp(rt->checkpoint) != 0)
    {
        goto vm_exit;
    }

    while (!rt->flag.gui_ready)
    {
        lua_pushcfunction(L, auto_lua_sleep);
        lua_pushinteger(L, 10);
        lua_call(L, 1, 0);
    }

    lua_pushcfunction(L, _lua_run);
    int ret = lua_pcall(L, 0, 0, 0);
    if (ret != LUA_OK)
    {
        fprintf(stderr, "%s:%d:%s\n",
            __FUNCTION__, __LINE__, lua_tostring(L, -1));
    }

vm_exit:
    /* Ask gui to exit */
    auto_gui_exit();
    return NULL;
}

static void _on_gui_event(auto_gui_msg_t* msg, void* udata)
{
    auto_runtime_t* rt = udata;

    switch (msg->event)
    {
    case AUTO_GUI_READY:
        rt->flag.gui_ready = 1;
        break;

    case AUTO_GUI_QUIT:
        rt->flag.looping = 0;
        break;

    default:
        break;
    }
}

int main(int argc, char* argv[])
{
    lua_State* L = luaL_newstate();

    /* Initialize Lua VM */
    lua_pushcfunction(L, _lua_init_vm);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);

    /* Initialize failure */
    if (lua_pcall(L, 2, 1, 0) != LUA_OK)
    {
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
        return exit_ret;
    }

    auto_gui_startup_info_t info;
    memset(&info, 0, sizeof(info));
    info.argc = argc;
    info.argv = argv;
    info.udata = auto_get_runtime(L);
    info.on_event = _on_gui_event;

    pthread_t tid;
    pthread_create(&tid, NULL, _lua_routine, L);

    /* Initialize GUI */
    int ret = auto_gui(&info);

    /* Cleanup */
    pthread_join(tid, NULL);
    lua_close(L);

    return ret;
}
