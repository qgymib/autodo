#ifndef __AUTO_LUA_REGEX_H__
#define __AUTO_LUA_REGEX_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Regex
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_regex(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
