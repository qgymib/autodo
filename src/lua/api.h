#ifndef __AUTO_LUA_API_H__
#define __AUTO_LUA_API_H__

#include "autodo.h"

#define ERR_HINT_NOT_IN_MANAGED_COROUTINE   "you are not in managed coroutine"
#define ERR_HINT_STDOUT_DISABLED            "stdout have been disabled"
#define ERR_HINT_STDIN_DISABLED             "stdin have been disabled"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exposed api for memory.
 */
AUTO_LOCAL extern const auto_api_memory_t api_memory;

/**
 * @brief Exposed api for list.
 */
AUTO_LOCAL extern const auto_api_list_t api_list;

/**
 * @brief Exposed api for map.
 */
AUTO_LOCAL extern const auto_api_map_t api_map;

/**
 * @brief Exposed api for misc.
 */
AUTO_LOCAL extern const auto_api_misc_t api_misc;

/**
 * @brief Exposed api for sem.
 */
AUTO_LOCAL extern const auto_api_sem_t api_sem;

/**
 * @brief Exposed api for thread.
 */
AUTO_LOCAL extern const auto_api_thread_t api_thread;

/**
 * @brief Exposed api for timer.
 */
AUTO_LOCAL extern const auto_api_timer_t api_timer;

/**
 * @brief Exposed api for async.
 */
AUTO_LOCAL extern const auto_api_async_t api_async;

/**
 * @brief Exposed API.
 */
AUTO_LOCAL extern const auto_api_t api;

/**
 * @brief Initialize builtin library.
 * @param[in] L     Lua VM.
 */
AUTO_LOCAL void auto_init_libs(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif
