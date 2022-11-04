#define AUTO_API_EXPORT
#include <string.h>
#include "runtime.h"
#include "api.h"
#include "api/coroutine.h"
#include "api/int64.h"
#include "api/list.h"
#include "api/map.h"
#include "api/memory.h"
#include "api/misc.h"
#include "api/notify.h"
#include "api/regex.h"
#include "api/sem.h"
#include "api/thread.h"
#include "api/timer.h"
#include "lua/coroutine.h"
#include "lua/download.h"
#include "lua/int64.h"
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
    xx("int64",             auto_lua_int64)         \
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

    static struct { const char* name; void* value; } s_api_list[] = {
        { "c_api",              (void*)&api },
        { "c_api_memory",       (void*)&api_memory },
        { "c_api_list",         (void*)&api_list },
        { "c_api_map",          (void*)&api_map },
        { "c_api_misc",         (void*)&api_misc },
        { "c_api_sem",          (void*)&api_sem },
        { "c_api_thread",       (void*)&api_thread },
        { "c_api_coroutine",    (void*)&api_coroutine },
        { "c_api_timer",        (void*)&api_timer },
        { "c_api_notify",       (void*)&api_notify },
        { "c_api_int64",        (void*)&api_int64 },
        { "c_api_regex",        (void*)&api_regex },
    };

    /* Register C API */
    size_t i;
    for (i = 0; i < ARRAY_SIZE(s_api_list); i++)
    {
        lua_pushlightuserdata(L, s_api_list[i].value);
        lua_setfield(L, -2, s_api_list[i].name);
    }

    /* Set as global variable */
    lua_setglobal(L, "auto");
}

/******************************************************************************
* C API: .async
******************************************************************************/

const auto_api_t api = {
    &api_memory,
    &api_list,
    &api_map,
    &api_sem,
    &api_thread,
    &api_timer,
    &api_notify,
    &api_coroutine,
    &api_int64,
    &api_misc,
    &api_regex,
};
