#ifndef __AUTO_LUA_API_H__
#define __AUTO_LUA_API_H__

#include "api/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize builtin library.
 * @param[in] L     Lua VM.
 */
AUTO_LOCAL void auto_init_libs(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif
