#ifndef __AUTO_LUA_API_H__
#define __AUTO_LUA_API_H__

#include "autodo.h"

#define ERR_HINT_NOT_IN_MANAGED_COROUTINE   "you are not in managed coroutine"
#define ERR_HINT_STDOUT_DISABLED            "stdout have been disabled"
#define ERR_HINT_STDIN_DISABLED             "stdin have been disabled"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exposed API.
 */
AUTO_LOCAL extern const auto_api_t api;

/**
 * @brief Initialize builtin library.
 * @param[in] L     Lua VM.
 */
AUTO_LOCAL void auto_init_libs(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif
