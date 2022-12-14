#ifndef __AUTO_LUA_PROCESS_H__
#define __AUTO_LUA_PROCESS_H__

#include <uv.h>
#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create process.
 *
 * ```
 * {
 *     "file": "file",
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
AUTO_LOCAL int atd_lua_process(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
