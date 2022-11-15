#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "runtime.h"
#include "fs.h"
#include "utils.h"

#if defined(_WIN32)
#else
#include <sys/stat.h>
#endif

#define AUTO_FS_LISTDIR_ITER   "__auto_fs_listdir_iterator"

typedef struct fs_listdir_record
{
    auto_list_node_t    node;           /**< List node. */

    const char*         path;           /**< Data content of path_ref. */
    int                 path_ref;       /**< Reference to current path string. */

#if defined(_WIN32)
    HANDLE              dp;             /**< Directory pointer. */
    WIN32_FIND_DATAA    entry;          /**< The entry we are listing. */
    int                 findret;        /**< Validity of entry. */
#else
    DIR*                dp;             /**< Directory pointer. */
    struct dirent*      entry;          /**< The entry we are listing. */
#endif
} fs_listdir_record_t;

typedef struct fs_listdir_helper
{
    int                 root_path_ref;  /**< Reference to the root directory string. */
    auto_list_t         dir_queue;      /**< Directory queue */
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

static void _lua_fs_listdir_destroy_record(lua_State* L,
    fs_listdir_helper_t* helper, fs_listdir_record_t* rec)
{
    ev_list_erase(&helper->dir_queue, &rec->node);
#if defined(_WIN32)
    if (rec-> dp != INVALID_HANDLE_VALUE)
    {
        FindClose(rec-> dp);
        rec->dp = INVALID_HANDLE_VALUE;
    }
#else
    if (rec->dp != NULL)
    {
        closedir(rec->dp);
        rec->dp = NULL;
        rec->entry = NULL;
    }
#endif
    if (rec->path_ref != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, rec->path_ref);
        rec->path_ref = LUA_NOREF;
        rec->path = NULL;
    }
    free(rec);
}

static int _lua_fs_listdir_do_one_last_record(lua_State* L, fs_listdir_helper_t* helper)
{
    auto_list_node_t* it = ev_list_end(&helper->dir_queue);
    if (it == NULL)
    {
        return 0;
    }
    fs_listdir_record_t* rec = container_of(it, fs_listdir_record_t, node);

#if defined(_WIN32)

    if (rec->findret && (rec->entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        /* Store new path. */
        lua_pushfstring(L, "%s/%s", rec->path, rec->entry.cFileName);

        /* Create child node */
        rec = malloc(sizeof(fs_listdir_record_t));
        rec->path = lua_tostring(L, -1);
        rec->path_ref = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_pushfstring(L, "%s/*", rec->path);
        rec->dp = FindFirstFile(lua_tostring(L, -1), &rec->entry);
        lua_pop(L, 1);

        rec->findret = rec->dp != INVALID_HANDLE_VALUE;
    }

    if (!rec->findret)
    {
        goto remove_current_node;
    }

    do
    {
        if (strcmp(rec->entry.cFileName, ".") == 0 || strcmp(rec->entry.cFileName, "..") == 0)
        {
            continue;
        }

        /* Push return value. */
        lua_pushlightuserdata(L, rec);
        lua_pushfstring(L, "%s/%s", rec->path, rec->entry.cFileName);

        /* Point to next node. */
        rec->findret = FindNextFile(rec->dp, &rec->entry);

        return 2;
    } while ((rec->findret = FindNextFile(rec->dp, &rec->entry)) != 0);

#else
    /* If it is a directory, jump into it. */
    if (rec->entry != NULL && rec->entry->d_type == DT_DIR)
    {
        /* Store new path. */
        lua_pushfstring(L, "%s/%s", rec->path, rec->entry->d_name);

        /* Reset to avoid enter again. */
        rec->entry = NULL;

        /* Create child node */
        rec = malloc(sizeof(fs_listdir_record_t));
        rec->path = lua_tostring(L, -1);
        rec->path_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        rec->dp = opendir(rec->path);
        rec->entry = NULL;
        ev_list_push_back(&helper->dir_queue, &rec->node);
    }

    if (rec->dp == NULL)
    {
        goto remove_current_node;
    }

    /* Checkout available node. */
    while ((rec->entry = readdir(rec->dp)) != NULL)
    {
        if (strcmp(rec->entry->d_name, ".") == 0 || strcmp(rec->entry->d_name, "..") == 0)
        {
            continue;
        }

        lua_pushlightuserdata(L, rec);
        lua_pushfstring(L, "%s/%s", rec->path, rec->entry->d_name);
        return 2;
    }

#endif

remove_current_node:
    /* No more file, need to revert to upper folder. */
    _lua_fs_listdir_destroy_record(L, helper, rec);
    return 0;
}

static int _lua_fs_listdir_finish(lua_State* L, fs_listdir_helper_t* helper)
{
    int ret;
    while (ev_list_size(&helper->dir_queue) != 0)
    {
        if ((ret = _lua_fs_listdir_do_one_last_record(L, helper)) != 0)
        {
            return ret;
        }
    }

    return 0;
}

static int _lua_fs_listdir_first(lua_State* L, fs_listdir_helper_t* helper)
{
    fs_listdir_record_t* rec = malloc(sizeof(fs_listdir_record_t));

    lua_rawgeti(L, LUA_REGISTRYINDEX, helper->root_path_ref);
    rec->path = lua_tostring(L, -1);
    rec->path_ref = luaL_ref(L, LUA_REGISTRYINDEX);
#if defined(_WIN32)
    lua_pushfstring(L, "%s/*", rec->path);
    rec->dp = FindFirstFile(lua_tostring(L, -1), &rec->entry);
    lua_pop(L, 1);
    rec->findret = rec->dp != INVALID_HANDLE_VALUE;
#else
    rec->entry = NULL;
    rec->dp = opendir(rec->path);
#endif
    ev_list_push_back(&helper->dir_queue, &rec->node);

    return _lua_fs_listdir_finish(L, helper);
}

static int _lua_fs_listdir_second(lua_State* L, fs_listdir_helper_t* helper)
{
    /* Following iterator */
    fs_listdir_record_t* rec = lua_touserdata(L, 2);
    assert(&rec->node == ev_list_end(&helper->dir_queue)); (void)rec;

    return _lua_fs_listdir_finish(L, helper);
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

    /* First iterator */
    if (lua_type(L, 2) == LUA_TNIL)
    {
        return _lua_fs_listdir_first(L, helper);
    }

    return _lua_fs_listdir_second(L, helper);
}

static int _lua_fs_listdir_gc(lua_State* L)
{
    fs_listdir_helper_t* helper = lua_touserdata(L, 1);

    auto_list_node_t* it;
    while ((it = ev_list_begin(&helper->dir_queue)) != NULL)
    {
        fs_listdir_record_t* rec = container_of(it, fs_listdir_record_t, node);
        _lua_fs_listdir_destroy_record(L, helper, rec);
    }

    if (helper->root_path_ref != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, helper->root_path_ref);
        helper->root_path_ref = LUA_NOREF;
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
    luaL_checktype(L, 1, LUA_TSTRING);

    /* 1: Push iterator function */
    lua_pushcfunction(L, _lua_fs_listdir_iter);

    /* 2: Iterator context */
    fs_listdir_helper_t* helper = lua_newuserdata(L, sizeof(fs_listdir_helper_t));
    memset(helper, 0, sizeof(*helper));
    _fs_listdir_setmetatable(L);

    lua_pushvalue(L, 1);
    helper->root_path_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    ev_list_init(&helper->dir_queue);

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
