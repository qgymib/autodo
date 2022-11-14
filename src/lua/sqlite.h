#ifndef __AUTO_LUA_API_SQLITE_H__
#define __AUTO_LUA_API_SQLITE_H__
#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new SQLite instance.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_sqlite(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
