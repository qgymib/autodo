#include <uv.h>
#include "uname.h"

int auto_lua_uname(lua_State *L)
{
    uv_utsname_t buf;
    if (uv_os_uname(&buf) != 0)
    {
        return 0;
    }

    lua_newtable(L);
    lua_pushstring(L, buf.machine);
    lua_setfield(L, -2, "machine");
    lua_pushstring(L, buf.release);
    lua_setfield(L, -2, "release");
    lua_pushstring(L, buf.sysname);
    lua_setfield(L, -2, "sysname");
    lua_pushstring(L, buf.version);
    lua_setfield(L, -2, "version");

    return 1;
}
