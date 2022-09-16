#ifndef __AUTO_LUA_API_H__
#define __AUTO_LUA_API_H__

#include "autodo.h"

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#   define API_LOCAL    __attribute__((visibility ("hidden")))
#else
#   define API_LOCAL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Exposed api.
*/
extern atd_api_t api;

/**
 * @brief Initialize builtin library.
 * @param[in] L     Lua VM.
 */
API_LOCAL void auto_init_libs(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif
