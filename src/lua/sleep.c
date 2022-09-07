#include "runtime.h"
#include "sleep.h"

int auto_lua_sleep(lua_State *L)
{
    auto_runtime_t* rt = auto_get_runtime(L);
    uint32_t timeout = (uint32_t)lua_tointeger(L, -1);

    /* Get current time */
    uv_update_time(&rt->loop);
    uint64_t now_time = uv_now(&rt->loop);
    /* Calculate timeout */
    uint64_t dst_time = now_time + timeout;

    while (1)
    {
        AUTO_CHECK_TERM(rt);

        uv_update_time(&rt->loop);
        now_time = uv_now(&rt->loop);
        if (now_time >= dst_time)
        {
            break;
        }

        /* Max sleep 10 ms */
        uint64_t dif_time = dst_time - now_time;
        if (dif_time > 10)
        {
            dif_time = 10;
        }
        uv_sleep(dif_time);
    }

    return 0;
}
