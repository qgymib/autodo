#ifndef __AUTO_LUA_API_H__
#define __AUTO_LUA_API_H__

#include "autodo.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Exposed api.
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
