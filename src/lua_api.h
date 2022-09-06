#ifndef __AUTODO_LUA_API_H__
#define __AUTODO_LUA_API_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/**
 * @brief Initialize builtin library.
 * @param[in] L     Lua VM.
 */
void auto_init_libs(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif
