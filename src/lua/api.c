#include <string.h>
#include "api.h"
#include "lua/coroutine.h"
#include "lua/download.h"
#include "lua/fs.h"
#include "lua/json.h"
#include "lua/process.h"
#include "lua/regex.h"
#include "lua/sleep.h"
#include "lua/sqlite.h"
#include "lua/uname.h"

/******************************************************************************
* Expose lua api and c api to lua vm
******************************************************************************/

/**
 * @brief Lua API list.
 */
#define AUTO_LUA_API_MAP(xx) \
    xx("coroutine",         auto_new_coroutine)     \
    xx("download",          auto_lua_download)      \
    xx("fs_abspath",        auto_lua_fs_abspath)    \
    xx("fs_basename",       auto_lua_fs_basename)   \
    xx("fs_delete",         auto_lua_fs_delete)     \
    xx("fs_dirname",        auto_lua_fs_dirname)    \
    xx("fs_expand",         auto_lua_fs_expand)     \
    xx("fs_isfile",         auto_lua_fs_isfile)     \
    xx("fs_isdir",          auto_lua_fs_isdir)      \
    xx("fs_listdir",        auto_lua_fs_listdir)    \
    xx("fs_mkdir",          auto_lua_fs_mkdir)      \
    xx("fs_splitpath",      auto_lua_fs_splitpath)  \
    xx("json",              auto_lua_json)          \
    xx("process",           atd_lua_process)        \
    xx("regex",             auto_lua_regex)         \
    xx("sleep",             atd_lua_sleep)          \
    xx("sqlite",            auto_lua_sqlite)        \
    xx("uname",             auto_lua_uname)

#define EXPAND_MAP_AS_LUA_FUNCTION(name, func) \
    { name, func },

static const luaL_Reg s_funcs[] = {
    AUTO_LUA_API_MAP(EXPAND_MAP_AS_LUA_FUNCTION)
    { NULL, NULL }
};

#undef EXPAND_MAP_AS_LUA_FUNCTION

void auto_init_libs(lua_State *L)
{
    /* Create lua api table */
    luaL_newlib(L, s_funcs);

    /* Set as global variable */
    lua_setglobal(L, "auto");
}
