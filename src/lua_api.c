#include "lua_api.h"

static const luaL_Reg s_funcs[] = {
    { NULL, NULL },
};

void auto_init_libs(lua_State *L)
{
    luaL_newlib(L, s_funcs);
    lua_setglobal(L, "auto");
}
