#include <stdlib.h>
#include "runtime.h"
#include "fs.h"

int auto_lua_fs_abspath(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    char* real_path =
#if defined(_WIN32)
        _fullpath(NULL, path, 0);
#else
        realpath(path, NULL);
#endif

    if (real_path == NULL)
    {
        return 0;
    }

    lua_pushstring(L, real_path);
    free(real_path);

    return 1;
}

int auto_lua_fs_expand(lua_State* L)
{
#define QUICK_GSUB(p, r) \
    do {\
        const char* path = lua_tostring(L, -1);\
        luaL_gsub(L, path, p, r);\
        lua_remove(L, -2);\
    } while (0)

    auto_runtime_t* rt = auto_get_runtime(L);
    lua_pushvalue(L, 1);

    const size_t buf_size = 4096;
    char* buf = malloc(buf_size);

    QUICK_GSUB("$AUTO_SCRIPT_FILE", rt->config.script_file);
    QUICK_GSUB("$AUTO_SCRIPT_PATH", rt->config.script_path);
    QUICK_GSUB("$AUTO_SCRIPT_NAME", rt->config.script_name);

    size_t tmp_buf_size = buf_size;
    uv_cwd(buf, &tmp_buf_size);
    QUICK_GSUB("$AUTO_CWD", buf);

    tmp_buf_size = buf_size;
    uv_exepath(buf, &tmp_buf_size);
    QUICK_GSUB("$AUTO_EXE_PATH", buf);

    free(buf);
    return 1;

#undef QUICK_GSUB
}
