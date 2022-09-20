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
AUTO_LOCAL int atd_lua_process(lua_State *L);

AUTO_LOCAL atd_process_t* api_process_create(atd_process_cfg_t* cfg);
AUTO_LOCAL void api_process_kill(atd_process_t* self, int signum);
AUTO_LOCAL int api_process_send_to_stdin(atd_process_t* self, void* data,
    size_t size, atd_process_stdio_fn cb, void* arg);

#ifdef __cplusplus
}
#endif

#endif
