#ifndef __AUTO_LUA_FS_H__
#define __AUTO_LUA_FS_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get absolute path.
 * @param[in] L     Lua VM.
 * @return          1 if absolute path push on stack, 0 if not.
 */
AUTO_LOCAL int auto_lua_fs_abspath(lua_State* L);

/**
 * @brief Expand a path template.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_fs_expand(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
