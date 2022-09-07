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

#define PROBE       "AUTOMATION"

typedef struct auto_probe
{
    char                probe[1024];    /**< Probe */
} auto_probe_t;

static void _print_usage(const char* name)
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
    printf(s_usage, name, name);
}

static int _init_parse_args_finalize(lua_State* L, int idx, const char* name)
{
    int sp = lua_gettop(L);

    /* SP + 1 */
    int type_script_path = lua_getfield(L, idx, "script_path");
    /* SP + 2 */
    int type_compile_path = lua_getfield(L, idx, "compile_path");
    /* SP + 3 */
    int type_script = lua_getfield(L, idx, "script");
    /* SP + 4 */
    int type_output_path = lua_getfield(L, idx, "output_path");

    if (type_script_path != LUA_TNIL && type_compile_path != LUA_TNIL)
    {
        return luaL_error(L, "Conflict option: script followed by `-c`");
    }

    if (type_script_path == LUA_TNIL && type_compile_path == LUA_TNIL && type_script == LUA_TNIL)
    {
        _print_usage(name);
        lua_settop(L, sp);
        return 1;
    }

    if (type_compile_path != LUA_TNIL && type_output_path == LUA_TNIL)
    {
        const char* compile_path = lua_tostring(L, sp + 2);
        const char* ext = get_filename_ext(compile_path);

#if defined(_WIN32)
        size_t path_len = strlen(compile_path);
        size_t ext_len = strlen(ext);
        size_t malloc_size = path_len - ext_len + 4;
        char* output_path = malloc(malloc_size);
        assert(output_path != NULL);
        memcpy(output_path, compile_path, path_len - ext_len);
        memcpy(output_path + path_len - ext_len, "exe", 4);
        lua_pushstring(L, output_path);
        free(output_path);
#else
        size_t offset = ext - compile_path;
        char* output_path = strdup(compile_path);
        output_path[offset - 1] = '\0';
        lua_pushstring(L, output_path);
        free(output_path);
#endif
        lua_setfield(L, idx, "output_path");
    }

    lua_settop(L, sp);
    return 0;
}

static int _init_parse_args(lua_State* L, int idx)
{
    int i;

    int argc = lua_tointeger(L, 1);
    char** argv = lua_touserdata(L, 2);

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            _print_usage(argv[0]);
            return 1;
        }

        if (strcmp(argv[i], "-c") == 0)
        {
            i++;
            if (i >= argc)
            {
                return luaL_error(L, "missing argument to `-c`.");
            }

            lua_pushstring(L, argv[i]);
            lua_setfield(L, idx, "compile_path");

            continue;
        }

        if (strcmp(argv[i], "-o") == 0)
        {
            i++;
            if (i >= argc)
            {
                return luaL_error(L, "missing argument to `-o`.");
            }

            lua_pushstring(L, argv[i]);
            lua_setfield(L, idx, "output_path");
            continue;
        }

        lua_pushstring(L, argv[i]);
        lua_setfield(L, idx, "script_path");
    }

    return _init_parse_args_finalize(L, idx, argv[0]);
}

static int _read_self(void** data, size_t* size)
{
    return auto_readfile("/proc/self/exe", data, size);
}

static void _init_probe(auto_probe_t* probe)
{
    memset(probe, '=', sizeof(*probe));
    memcpy(probe->probe, PROBE, sizeof(PROBE));
    probe->probe[sizeof(probe->probe) - 1] = '\0';
}

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
        _read_self(&exe_data, &exe_size);
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

static void _lua_init_vm_arg(lua_State* L, int idx)
{
    int i;

    lua_newtable(L);

    int argc = lua_tointeger(L, 1);
    char** argv = lua_touserdata(L, 2);
    for (i = 0; i < argc; i++)
    {
        lua_pushstring(L, argv[i]);
        lua_seti(L, -2, i + 1);
    }

    lua_setfield(L, idx, "arg");
}

static int _lua_init_script(lua_State* L, int idx)
{
    int ret;
    char buffer[1024];

    /* Generate probe data */
    auto_probe_t probe_data;
    _init_probe(&probe_data);

    void* data; size_t size;
    if ((ret = _read_self(&data, &size)) != 0)
    {
        return luaL_error(L, "read self failed: %s(%d)",
            auto_strerror(ret, buffer, sizeof(buffer)), ret);
    }

    int32_t fsm[sizeof(probe_data)];
    int script_offset = aeda_find(data, size, &probe_data, sizeof(probe_data),
        fsm, sizeof(probe_data));

    if (script_offset > 0)
    {
        script_offset += sizeof(probe_data);
        lua_pushlstring(L, (char*)data + script_offset, size - script_offset);
        lua_setfield(L, idx, "script");
    }

    free(data);

    return 0;
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

    int sp = lua_gettop(L);
    lua_newtable(L);

    /* table.runtime */
    auto_init_runtime(L, sp + 1);

    /* table.script: script self */
    _lua_init_script(L, sp + 1);

    /* table.arg: command line arguments*/
    _lua_init_vm_arg(L, sp + 1);

    /* Parser command line arguments */
    if (_init_parse_args(L, sp + 1))
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    /* Set as global variable */
    lua_setglobal(L, AUTO_GLOBAL);

    lua_pushboolean(L, 1);
    return 1;
}

static int _lua_run(lua_State* L)
{
    int sp = lua_gettop(L);

    /* SP + 1 */
    if (lua_getglobal(L, AUTO_GLOBAL) != LUA_TTABLE)
    {
        return luaL_error(L, "missing `%s`", AUTO_GLOBAL);
    }

    /* SP + 2 */
    if (lua_getfield(L, sp + 1, "script") != LUA_TNIL)
    {
        size_t size;
        const char* data = lua_tolstring(L, sp + 2, &size);

        /* SP + 3 */
        if (luaL_loadbuffer(L, data, size, "script") != LUA_OK)
        {
            return lua_error(L);
        }
        lua_call(L, 0, LUA_MULTRET);
        return 0;
    }
    /* SP + 1 */
    lua_pop(L, 1);

    /* SP + 2 */
    if (lua_getfield(L, sp + 1, "script_path") != LUA_TNIL)
    {
        return _run_script(L, lua_tostring(L, sp + 2));
    }
    /* SP + 1 */
    lua_pop(L, 1);

    /* SP + 2 */
    if (lua_getfield(L, sp + 1, "compile_path") != LUA_TNIL)
    {
        /* SP + 3 */
        lua_getfield(L, sp + 1, "output_path");
        return _compile_script(L, lua_tostring(L, sp + 2), lua_tostring(L, sp + 3));
    }
    /* SP + 1 */
    lua_pop(L, 1);

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

    while (!rt->flag_gui_ready)
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
        rt->flag_gui_ready = 1;
        break;

    case AUTO_GUI_QUIT:
        rt->looping = 0;
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
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_close(L);
        return EXIT_FAILURE;
    }

    /* Initialize stage require to exit normally */
    if (!lua_toboolean(L, -1))
    {
        lua_close(L);
        return EXIT_SUCCESS;
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
