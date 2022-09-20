#ifndef __AUTO_LUA_COROUTINE_H__
#define __AUTO_LUA_COROUTINE_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run function in new coroutine.
 * @param[in] L     Lua VM.
 * @return          Always 0.
 */
AUTO_LOCAL int atd_lua_coroutine(lua_State *L);

AUTO_LOCAL atd_coroutine_t* api_coroutine_find(lua_State* L);
AUTO_LOCAL atd_coroutine_t* api_coroutine_host(lua_State* L);
AUTO_LOCAL void api_coroutine_unhook(struct atd_coroutine* self, atd_coroutine_hook_t* token);
AUTO_LOCAL atd_coroutine_hook_t* api_coroutine_hook(struct atd_coroutine* self, atd_coroutine_hook_fn fn, void* arg);
AUTO_LOCAL void api_coroutine_set_state(struct atd_coroutine* self, int state);

#ifdef __cplusplus
}
#endif

#endif
