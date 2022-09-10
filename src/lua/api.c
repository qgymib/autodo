#include "runtime.h"
#include "lua/coroutine.h"
#include "lua/screenshot.h"
#include "lua/sleep.h"

/**
 * @brief Lua API list.
 */
#define AUTO_LUA_API_MAP(xx) \
    xx("coroutine",         atd_lua_coroutine)      \
    xx("screenshot",        atd_lua_screenshot)     \
    xx("sleep",             atd_lua_sleep)

/**
 * Generate proxy function for check if we need stop script.
 * @{
 */
#define EXPAND_MAP_AS_PROXY_FUNCTION(name, func)    \
    static int func##_##proxy(lua_State *L) {\
        atd_runtime_t* rt = atd_get_runtime(L);\
        AUTO_CHECK_TERM(rt);\
        return func(L);\
    }

AUTO_LUA_API_MAP(EXPAND_MAP_AS_PROXY_FUNCTION)

#undef EXPAND_MAP_AS_PROXY_FUNCTION
/**
 * @}
 */

/**
 * Generate lua function list.
 * @{
 */
#define EXPAND_MAP_AS_LUA_FUNCTION(name, func) \
    { name, func##_##proxy },

static const luaL_Reg s_funcs[] = {
    AUTO_LUA_API_MAP(EXPAND_MAP_AS_LUA_FUNCTION)
    { NULL, NULL }
};

#undef EXPAND_MAP_AS_LUA_FUNCTION
/**
 * @}
 */

void auto_init_libs(lua_State *L)
{
    luaL_newlib(L, s_funcs);
    lua_setglobal(L, "auto");
}
