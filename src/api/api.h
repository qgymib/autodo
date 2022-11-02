#ifndef __AUTO_API_H__
#define __AUTO_API_H__

#include <autodo.h>

#if 1
#define static_assert(e, m) _Static_assert(e, m)
#else
#define static_assert(e, m) \
    do { enum { assert_static__ = 1/(e) }; } while (0)
#endif

#define ERR_HINT_NOT_IN_MANAGED_COROUTINE   "you are not in managed coroutine"
#define ERR_HINT_STDOUT_DISABLED            "stdout have been disabled"
#define ERR_HINT_STDIN_DISABLED             "stdin have been disabled"
#define ERR_HINT_DEFINITION_MISMATCH        "definition mismatch"

#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/**
* @brief Exposed API.
*/
AUTO_LOCAL extern const auto_api_t api;

#ifdef __cplusplus
}
#endif

#endif
