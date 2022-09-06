#include "api.h"
#include "screenshot.h"
#include "sleep.h"

static const luaL_Reg s_funcs[] = {
    { "take_screenshot", auto_lua_take_screenshot },
    { "sleep",           auto_lua_sleep },
    { NULL,                 NULL },
};

void auto_init_libs(lua_State *L)
{
    luaL_newlib(L, s_funcs);
    lua_setglobal(L, "auto");
}
