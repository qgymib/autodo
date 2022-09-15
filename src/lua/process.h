#ifndef __AUTO_LUA_PROCESS_H__
#define __AUTO_LUA_PROCESS_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create process.
 *
 * ```
 * {
 *     "path": "file",
 *     "cwd": "working directory",
 *     "args": ["file", "--command", "--list"],
 *     "envs": ["environment", "list"],
 *     "stdio": ["enable_stdin", "enable_stdout", "enable_stderr"],
 * }
 * ```
 *
 * @param L
 * @return
 */
int atd_lua_process(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
