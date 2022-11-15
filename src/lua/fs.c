#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "runtime.h"
#include "fs.h"
#include "utils.h"
#include "utils/fts.h"

#if defined(_WIN32)
#else
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

#if 0
static void _lua_fs_listdir_dump(lua_State* L, fs_listdir_helper_t* helper)
{
    size_t cnt;
    luaL_Buffer buf;

    auto_list_node_t* it = ev_list_begin(&helper->dir_queue);
    for (cnt = 0; it != NULL; it = ev_list_next(it), cnt++)
    {
        luaL_buffinit(L, &buf);

        fs_listdir_record_t* rec = container_of(it, fs_listdir_record_t, node);

        size_t i;
        for (i = 0; i < cnt; i++)
        {
            luaL_addchar(&buf, ' ');
        }
        luaL_addstring(&buf, rec->path);

        luaL_pushresult(&buf);
        AUTO_DEBUG("%s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}
#endif

static int _lua_fs_listdir_iter(lua_State* L)
{
    fs_listdir_helper_t* helper = lua_touserdata(L, 1);

    auto_fts_ent_t* ent = auto_fts_read(helper->fts);
    if (ent == NULL)
    {
        return 0;
    }

    lua_pushlightuserdata(L, NULL);
    lua_pushlstring(L, ent->name, ent->name_len);
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

    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

#if defined(_WIN32)
    int ret = stat_buf.st_mode & _S_IFREG;
#else
    int ret = S_ISREG(stat_buf.st_mode);
#endif
    lua_pushboolean(L, ret);
    return 1;
}
