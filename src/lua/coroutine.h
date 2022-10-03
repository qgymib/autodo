#ifndef __AUTO_LUA_COROUTINE_H__
#define __AUTO_LUA_COROUTINE_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new coroutine and push coroutine on top of stack.
 *
 * The first argument must be the coroutine function.
 * The second argument is optional, which is the function argument.
 *
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_new_coroutine(lua_State *L);

/**
 * @brief Find coroutine context by \p L.
 * @internal
 * @param[in] L Lua coroutine.
 * @return      Coroutine context.
 */
AUTO_LOCAL atd_coroutine_t* api_coroutine_find(lua_State* L);

/**
 * @brief Host Lua coroutine as managed coroutine.
 * @param[in] L Lua coroutine.
 * @return      Managed coroutine context.
 */
AUTO_LOCAL atd_coroutine_t* api_coroutine_host(lua_State* L);

/**
 * @brief Register coroutine schedule hook.
 * @param[in] self  Managed coroutine context.
 * @param[in] fn    Schedule hook.
 * @param[in] arg   User defined argument passed to hook.
 * @return          Hook token.
 */
AUTO_LOCAL atd_coroutine_hook_t* api_coroutine_hook(struct atd_coroutine* self,
    atd_coroutine_hook_fn fn, void* arg);

/**
 * @brief Unregister coroutine schedule hook.
 * @param[in] self  Managed coroutine context.
 * @param[in] token Hook token returned by #api_coroutine_hook().
 */
AUTO_LOCAL void api_coroutine_unhook(struct atd_coroutine* self,
    atd_coroutine_hook_t* token);

/**
 * @brief Set coroutine schedule state.
 * @param[in] self  Managed coroutine context.
 * @param[in] state Schedule state.
 */
AUTO_LOCAL void api_coroutine_set_state(struct atd_coroutine* self, int state);

#ifdef __cplusplus
}
#endif

#endif
