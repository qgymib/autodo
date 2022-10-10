#ifndef __AUTO_LUA_COROUTINE_H__
#define __AUTO_LUA_COROUTINE_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Exposed api for coroutine.
*/
AUTO_LOCAL extern const auto_api_coroutine_t api_coroutine;

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

#ifdef __cplusplus
}
#endif

#endif
