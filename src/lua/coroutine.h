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
int atd_lua_coroutine(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
