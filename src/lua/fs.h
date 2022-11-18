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

/**
 * @brief List all files in directory.
 * @param[in] L     Lua VM.
 * @return          Always 3.
 */
AUTO_LOCAL int auto_lua_fs_iterdir(lua_State* L);

/**
 * @brief Check if \p path is a file.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_fs_isfile(lua_State* L);

/**
 * @brief Check if \p path is a directory.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_fs_isdir(lua_State* L);

/**
 * @brief Delete file or directory.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_fs_delete(lua_State* L);

/**
 * @brief Get filename.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_fs_basename(lua_State* L);

/**
 * @brief Get directory.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_fs_dirname(lua_State* L);

/**
 * @brief Split path into directory and filename components.
 * @param[in] L     Lua VM.
 * @return          Always 2.
 */
AUTO_LOCAL int auto_lua_fs_splitpath(lua_State* L);

/**
 * @brief Create directory.
 * @param[in] L     Lua VM.
 * @return          Always 0.
 */
AUTO_LOCAL int auto_lua_fs_mkdir(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
