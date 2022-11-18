#ifndef __AUTO_LUA_STRING_H__
#define __AUTO_LUA_STRING_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Split string into sequence.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_string_split(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
