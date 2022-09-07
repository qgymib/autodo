#include <string.h>
#include "runtime.h"

static int _runtime_gc(lua_State* L)
{
    int ret;
    auto_runtime_t* rt = lua_touserdata(L, 1);

    if ((ret = uv_loop_close(&rt->loop)) != 0)
    {
        return luaL_error(L, "close event loop failed: %d", ret);
    }
    return 0;
}

int auto_init_runtime(lua_State* L, int idx)
{
    auto_runtime_t* rt = lua_newuserdata(L, sizeof(auto_runtime_t));
    memset(rt, 0, sizeof(*rt));

    uv_loop_init(&rt->loop);
    rt->looping = 1;

    static const luaL_Reg s_runtime_meta[] = {
            { "__gc",   _runtime_gc },
            { NULL,     NULL },
    };
    if (luaL_newmetatable(L, "__auto_runtime") != 0)
    {
        luaL_setfuncs(L, s_runtime_meta, 0);
    }
    lua_setmetatable(L, -2);

    lua_setfield(L, idx, "runtime");

    return 0;
}

auto_runtime_t* auto_get_runtime(lua_State* L)
{
    int sp = lua_gettop(L);

    /* SP + 1 */
    lua_getglobal(L, AUTO_GLOBAL);
    /* SP + 2 */
    lua_getfield(L, sp + 1, "runtime");

    auto_runtime_t* rt = lua_touserdata(L, sp + 2);
    lua_settop(L, sp);

    return rt;
}
