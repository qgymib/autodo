#ifndef __AUTODO_LUA_API_H__
#define __AUTODO_LUA_API_H__

#include "autodo.h"

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
void auto_init_libs(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif
