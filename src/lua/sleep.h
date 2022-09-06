#ifndef __AUTO_LUA_SLEEP_H__
#define __AUTO_LUA_SLEEP_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sleep for millisecond.
 * @param[in] L Lua VM.
 * @return      0.
 */
int auto_lua_sleep(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
