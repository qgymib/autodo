#include "lua_api.h"
#include "lua_screenshot.h"
#include "lua_sleep.h"

static const luaL_Reg s_funcs[] = {
    { "take_screenshot",    auto_take_screenshot },
    { "sleep",              auto_sleep },
    { NULL,                 NULL },
};

void auto_init_libs(lua_State *L)
{
    luaL_newlib(L, s_funcs);
    lua_setglobal(L, "auto");
}
