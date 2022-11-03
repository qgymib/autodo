#define AUTO_API_EXPORT
#include <string.h>
#include "runtime.h"
#include "api.h"
#include "lua/coroutine.h"
#include "lua/download.h"
#include "lua/json.h"
#include "lua/process.h"
#include "lua/regex.h"
#include "lua/screenshot.h"
#include "lua/sleep.h"
#include "lua/uname.h"
#include "utils.h"

/******************************************************************************
* Expose lua api and c api to lua vm
******************************************************************************/

/**
 * @brief Lua API list.
 */
#define AUTO_LUA_API_MAP(xx) \
    xx("coroutine",         auto_new_coroutine)     \
    xx("download",          auto_lua_download)      \
    xx("json",              auto_lua_json)          \
    xx("process",           atd_lua_process)        \
    xx("regex",             auto_lua_regex)         \
    xx("screenshot",        atd_lua_screenshot)     \
    xx("sleep",             atd_lua_sleep)          \
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
