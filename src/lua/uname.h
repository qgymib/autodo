#ifndef __AUTO_LUA_UNAME_H__
#define __AUTO_LUA_UNAME_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief uname.
 * @param[in] L Lua VM.
 * @return      0.
 */
AUTO_LOCAL int auto_lua_uname(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
