#include <string.h>
#include <stdlib.h>
#include "runtime.h"
#include "utils.h"

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

static int _init_parse_args_finalize(lua_State* L, auto_runtime_t* rt, const char* name)
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
        const char* ext = get_filename_ext(rt->config.compile_path);

#if defined(_WIN32)
        size_t path_len = strlen(rt->config.compile_path);
        size_t ext_len = strlen(ext);
        size_t malloc_size = path_len - ext_len + 4;
        rt->config.output_path = malloc(malloc_size);
        assert(rt->config.output_path != NULL);
        memcpy(rt->config.output_path, rt->config.compile_path, path_len - ext_len);
        memcpy(rt->config.output_path + path_len - ext_len, "exe", 4);
#else
        size_t offset = ext - rt->config.compile_path;
        rt->config.output_path = strdup(rt->config.compile_path);
        rt->config.output_path[offset - 1] = '\0';
#endif
    }
    return 0;
}

static int _runtime_gc(lua_State* L)
{
    int ret;
    auto_runtime_t* rt = lua_touserdata(L, 1);

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

static int _init_runtime_script(lua_State* L, auto_runtime_t* rt)
{
    int ret;

    if ((ret = auto_read_self_script(&rt->script.data, &rt->script.size)) != 0)
    {
        return luaL_error(L, "read self failed: %s(%d)",
            auto_strerror(ret, rt->cache.errbuf, sizeof(rt->cache.errbuf)), ret);
    }

    return 0;
}

static int _init_parse_args(lua_State* L, auto_runtime_t* rt, int argc, char* argv[])
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
            rt->config.compile_path = auto_strdup(argv[i]);

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
            rt->config.output_path = strdup(argv[i]);

            continue;
        }

        if (rt->config.script_path != NULL)
        {
            free(rt->config.script_path);
        }
        rt->config.script_path = strdup(argv[i]);
    }

    return _init_parse_args_finalize(L, rt, argv[0]);
}

static void _init_runtime(lua_State* L, auto_runtime_t* rt, int argc, char* argv[])
{
    uv_loop_init(&rt->loop);
    rt->flag.looping = 1;

    _init_runtime_script(L, rt);
    _init_parse_args(L, rt, argc, argv);
}

int auto_init_runtime(lua_State* L, int argc, char* argv[])
{
    auto_runtime_t* rt = lua_newuserdata(L, sizeof(auto_runtime_t));
    memset(rt, 0, sizeof(*rt));

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

auto_runtime_t* auto_get_runtime(lua_State* L)
{
    int sp = lua_gettop(L);

    lua_getglobal(L, AUTO_GLOBAL);

    auto_runtime_t* rt = lua_touserdata(L, sp + 1);
    lua_settop(L, sp);

    return rt;
}
