#include "lua_sleep.h"

#if defined(_WIN32)

int auto_sleep(lua_State *L)
{
    uint32_t timeout = (uint32_t)lua_tointeger(L, -1);
    Sleep(timeout);
    return 0;
}

#else

#include <time.h>
#include <errno.h>

int auto_sleep(lua_State *L)
{
    uint32_t timeout = (uint32_t)lua_tointeger(L, -1);
    struct timespec t_req, t_rem;
    t_req.tv_sec = timeout / 1000;
    t_req.tv_nsec = (timeout - t_req.tv_sec * 1000) * 1000 * 1000;

    int ret;
    while((ret = nanosleep(&t_req, &t_rem)) != 0)
    {
        ret = errno;
        if (ret != EINTR)
        {
            return luaL_error(L, "nanosleep errno(%d)", errno);
        }
        t_req = t_rem;
    }

    return 0;
}

#endif
