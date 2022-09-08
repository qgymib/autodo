#include "runtime.h"
#include "coroutine.h"

int auto_lua_coroutine(lua_State *L)
{
    /* Get global runtime */
    auto_runtime_t* rt = auto_get_runtime(L);

    /* Create coroutine */
    auto_thread_t* thr = auto_new_thread(rt, L);

    /* Move function and arguments into coroutine */
    lua_xmove(L, thr->co, lua_gettop(L));

    return 0;
}
