#include "int64.h"
#include "api/int64.h"

int auto_lua_int64(lua_State* L)
{
    int val_type = lua_type(L, 1);
    if (val_type == LUA_TNUMBER)
    {
        api_int64.push_value(L, lua_tonumber(L, 1));
        return 1;
    }

    if (val_type == LUA_TNIL || val_type == LUA_TNONE)
    {
        api_int64.push_value(L, 0);
        return 1;
    }

    return luaL_typeerror(L, 1, lua_typename(L, LUA_TNUMBER));
}
