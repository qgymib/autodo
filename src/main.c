#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "lua_api.h"
#include "utils.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#define PROBE "AUTOMATION"

typedef struct runtime_s
{
    lua_State*  L;              /**< Lua VM */
    char        probe[1024];    /**< Probe */

    struct
    {
        void*   data;           /**< Executable content */
        size_t  size;           /**< Executable size */
        ssize_t script_offset;
    } exe;

    struct
    {
        int     argc;
        char**  argv;
    } startup;

    const char* script_path;
    const char* compile_path;
    char*       output_path;
} runtime_t;

static runtime_t g_rt;

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

static void _init_parse_args_finalize(void)
{
    if (g_rt.script_path != NULL && g_rt.compile_path != NULL)
    {
        fprintf(stderr, "Conflict option: script followed by `-c`\n");
        exit(EXIT_FAILURE);
    }
    if (g_rt.script_path == NULL && g_rt.compile_path == NULL && g_rt.exe.script_offset < 0)
    {
        _print_usage(g_rt.startup.argv[0]);
        exit(EXIT_SUCCESS);
    }
    if (g_rt.compile_path != NULL && g_rt.output_path == NULL)
    {
        const char* ext = get_filename_ext(g_rt.compile_path);
        size_t offset = ext - g_rt.compile_path;

#if defined(_WIN32)
        size_t path_len = strlen(g_rt.compile_path);
        size_t ext_len = strlen(ext);
        size_t malloc_size = path_len - ext_len + 4;
        g_rt.output_path = malloc(malloc_size);
        memcpy(g_rt.output_path, g_rt.compile_path, path_len - ext_len);
        memcpy(g_rt.output_path + path_len - ext_len, "exe", 4);
#else
        g_rt.output_path = strdup(g_rt.compile_path);
        g_rt.output_path[offset - 1] = '\0';
#endif
    }
}

static void _init_parse_args(int argc, char* argv[])
{
    int i;
    g_rt.startup.argc = argc;
    g_rt.startup.argv = argv;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            _print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }

        if (strcmp(argv[i], "-c") == 0)
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "missing argument to `-c`.\n");
                exit(EXIT_FAILURE);
            }
            g_rt.compile_path = argv[i];
            continue;
        }

        if (strcmp(argv[i], "-o") == 0)
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "missing argument to `-o`.\n");
                exit(EXIT_FAILURE);
            }
            if (g_rt.output_path != NULL)
            {
                free(g_rt.output_path);
            }
            g_rt.output_path = strdup(argv[i]);
            continue;
        }

        g_rt.script_path = argv[i];
    }

    _init_parse_args_finalize();
}

static void _init_probe(void)
{
    memset(g_rt.probe, '=', sizeof(g_rt.probe));
    memcpy(g_rt.probe, PROBE, sizeof(PROBE));
    g_rt.probe[sizeof(g_rt.probe) - 1] = '\0';
}

/**
 * @brief Read file content
 * @param[in] path  File path.
 * @param[out] data File content
 * @return          File size.
 */
static ssize_t _read_file(const char* path, void** data)
{
    FILE* exe = fopen(path, "rb");
    if (exe == NULL)
    {
        return -1;
    }

    fseek(exe, 0L, SEEK_END);
    size_t size = ftell(exe);
    fseek(exe, 0L, SEEK_SET);

    *data = malloc(size);
    fread(*data, size, 1, exe);
    fclose(exe);

    return (ssize_t)size;
}

static void _init_read_exe(void)
{
    ssize_t ret = _read_file("/proc/self/exe", &g_rt.exe.data);
    if (ret < 0)
    {
        fprintf(stderr, "open self failed.\n");
        exit(EXIT_FAILURE);
    }
    g_rt.exe.size = (size_t)ret;

    int32_t fsm[sizeof(g_rt.probe)];
    g_rt.exe.script_offset = aeda_find(g_rt.exe.data, g_rt.exe.size, g_rt.probe, sizeof(g_rt.probe),
        fsm, ARRAY_SIZE(fsm));
    if (g_rt.exe.script_offset > 0)
    {
        g_rt.exe.script_offset += sizeof(g_rt.probe);
    }
}

/**
 * @brief Initialize global runtime
 * @param[in] argc  The number of command line arguments
 * @param[in] argv  The list of command line arguments
 */
static void _init(int argc, char* argv[])
{
    g_rt.L = luaL_newstate();
    _init_probe();
    _init_read_exe();
    _init_parse_args(argc, argv);
}

static int _write_executable(lua_State* L, const char* dst)
{
    FILE* dst_file = fopen(dst, "wb");
    if (dst_file == NULL)
    {
        return luaL_error(L, "open `%s` failed: %s(%d).", dst, strerror(errno), errno);
    }

    size_t size;
    const char* data = lua_tolstring(L, -1, &size);

    fwrite(g_rt.exe.data, g_rt.exe.size, 1, dst_file);
    fwrite(g_rt.probe, sizeof(g_rt.probe), 1, dst_file);
    fwrite(data, size, 1, dst_file);

    fclose(dst_file);

#if !defined(_WIN32)
    chmod(dst, 0777);
#endif

    return 0;
}

static int _compile_script(lua_State* L, const char* src, const char* dst)
{
    int ret = luaL_loadfile(L, src);
    if (ret == LUA_ERRFILE)
    {
        return luaL_error(L, "open `%s` failed.", src);
    }
    if (ret != LUA_OK)
    {
        return lua_error(L);
    }

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    void* data;
    size_t size = _read_file(src, &data);
    luaL_addlstring(&buf, data, size);
    free(data);

    luaL_pushresult(&buf);

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

static int _run_self(lua_State* L, size_t offset)
{
    void* data = (void*)((uint8_t*)g_rt.exe.data + offset);
    size_t size = g_rt.exe.size - offset;

    if (luaL_loadbuffer(L, data, size, "script") != LUA_OK)
    {
        return lua_error(L);
    }

    return lua_pcall(L, 0, LUA_MULTRET, 0);
}

static int _main(lua_State* L)
{
    luaL_openlibs(L);
    auto_init_libs(L);

    /* If the program is self contains, execute embed script */
    if (g_rt.exe.script_offset > 0)
    {
        return _run_self(L, g_rt.exe.script_offset);
    }

    /* Execute script directly */
    if (g_rt.script_path != NULL)
    {
        return _run_script(L, g_rt.script_path);
    }

    /* Compile script */
    if (g_rt.compile_path != NULL)
    {
        return _compile_script(L, g_rt.compile_path, g_rt.output_path);
    }

    return 0;
}

static void _on_exit(void)
{
    if (g_rt.L != NULL)
    {
        lua_close(g_rt.L);
        g_rt.L = NULL;
    }

    if (g_rt.exe.data != NULL)
    {
        free(g_rt.exe.data);
        g_rt.exe.data = NULL;
    }
    g_rt.exe.size = 0;

    if (g_rt.output_path != NULL)
    {
        free(g_rt.output_path);
        g_rt.output_path = NULL;
    }
}

int main(int argc, char* argv[])
{
    /* memset runtime to avoid mess up exit process */
    memset(&g_rt, 0, sizeof(g_rt));
    /* Register global exit cleanup function */
    atexit(_on_exit);
    /* Initialize global runtime */
    _init(argc, argv);

    /* Enter program body */
    lua_pushcfunction(g_rt.L, _main);
    int ret = lua_pcall(g_rt.L, 0, 0, 0);
    if (ret != LUA_OK)
    {
        fprintf(stderr, "%s", lua_tostring(g_rt.L, -1));
    }

    /* Finish and cleanup */
    return ret;
}
