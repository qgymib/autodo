#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "runtime.h"
#include "fs.h"
#include "utils.h"
#include "utils/fts.h"
#include "utils/mkdir.h"

#if defined(_WIN32)
#else
#include <libgen.h>
#include <sys/stat.h>
#endif

#define AUTO_FS_LISTDIR_ITER   "__auto_fs_listdir_iterator"

typedef struct fs_listdir_helper
{
    auto_fts_t*         fts;    /**< Filesystem Traversing Stream. */
} fs_listdir_helper_t;

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

static int _lua_fs_listdir_iter(lua_State* L)
{
    fs_listdir_helper_t* helper = lua_touserdata(L, 1);

    auto_fts_ent_t* ent = auto_fts_read(helper->fts);
    if (ent == NULL)
    {
        return 0;
    }

    lua_pushlightuserdata(L, NULL);
    lua_pushlstring(L, ent->path, ent->path_len);
    return 2;
}

static int _lua_fs_listdir_gc(lua_State* L)
{
    fs_listdir_helper_t* helper = lua_touserdata(L, 1);

    if (helper->fts != NULL)
    {
        auto_fts_close(helper->fts);
        helper->fts = NULL;
    }

    return 0;
}

static void _fs_listdir_setmetatable(lua_State* L)
{
    static const luaL_Reg s_sqlite_meta[] = {
        { "__gc", _lua_fs_listdir_gc },
        { NULL,     NULL },
    };
    if (luaL_newmetatable(L, AUTO_FS_LISTDIR_ITER) != 0)
    {
        luaL_setfuncs(L, s_sqlite_meta, 0);
    }
    lua_setmetatable(L, -2);
}

int auto_lua_fs_listdir(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    /* 1: Push iterator function */
    lua_pushcfunction(L, _lua_fs_listdir_iter);

    /* 2: Iterator context */
    fs_listdir_helper_t* helper = lua_newuserdata(L, sizeof(fs_listdir_helper_t));
    helper->fts = auto_fts_open(path, AUTO_FTS_POST_ORDER);
    _fs_listdir_setmetatable(L);

    /* 3: Nil required by `for ... in` syntax */
    lua_pushnil(L);

    return 3;
}

int auto_lua_fs_isfile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    int ret = auto_isfile(path);

    lua_pushboolean(L, ret == 0);
    return 1;
}

int auto_lua_fs_isdir(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    int ret = auto_isdir(path);

    lua_pushboolean(L, ret == 0);
    return 1;
}

int auto_lua_fs_delete(lua_State* L)
{
    int ret = 1;
    const char* path = luaL_checkstring(L, 1);
    int recursion = 0;
    if (lua_type(L, 2) == LUA_TBOOLEAN)
    {
        recursion = lua_toboolean(L, 2);
    }

    lua_pushcfunction(L, auto_lua_fs_isfile);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);

    if (lua_toboolean(L, -1) || !recursion)
    {
        ret = remove(path);
        lua_pushboolean(L, ret == 0);
        return 1;
    }

    auto_fts_ent_t* ent;
    auto_fts_t* fts = auto_fts_open(path, AUTO_FTS_POST_ORDER);
    while ((ent = auto_fts_read(fts)) != NULL)
    {
        if (remove(ent->path) != 0)
        {
            ret = 0;
        }
    }
    auto_fts_close(fts);

    lua_pushboolean(L, ret);
    return 1;
}

int auto_lua_fs_basename(lua_State* L)
{
    lua_settop(L, 1);

    lua_pushcfunction(L, auto_lua_fs_splitpath);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 2);

    return 1;
}

int auto_lua_fs_dirname(lua_State* L)
{
    lua_settop(L, 1);

    lua_pushcfunction(L, auto_lua_fs_splitpath);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 2);

    lua_pop(L, 1);
    return 1;
}

int auto_lua_fs_splitpath(lua_State* L)
{
    size_t path_sz;
    const char* path = luaL_checklstring(L, 1, &path_sz);

    const char* p = path + path_sz;
    while (*p != '/' && *p != '\\')
    {
        if (p == path)
        {
            lua_pushstring(L, "");
            lua_pushvalue(L, -1);
            return 2;
        }
        p--;
    }

    lua_pushlstring(L, path, p - path);
    lua_pushstring(L, p + 1);
    return 2;
}

int auto_lua_fs_mkdir(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    int parents = 0;
    if (lua_type(L, 2) == LUA_TBOOLEAN)
    {
        parents = lua_toboolean(L, 2);
    }

    int errcode = auto_mkdir(path, parents);
    if (errcode != 0)
    {
        char buf[128];
        return luaL_error(L, "%s", auto_strerror(errcode, buf, sizeof(buf)));
    }

    return 0;
}
