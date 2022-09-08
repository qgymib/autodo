#ifndef __AUTO_RUNTIME_H__
#define __AUTO_RUNTIME_H__

#include <uv.h>
#include <setjmp.h>
#include "lua/api.h"

/**
 * @brief Global runtime.
 *
 * ```
 * {
 *     "arg": [ "command", "line", "arguments" ],
 *     "runtime": auto_runtime_t,
 *     "script": "script self",
 *     "compile_path": "path to source file to compile",
 *     "output_path": "path to output file",
 *     "script_path": "path to script file"
 * }
 * ```
 */
#define AUTO_GLOBAL "_AUTO_G"

/**
 * @brief Terminate script if necessary.
 * @param[in] rt    Runtime.
 */
#define AUTO_CHECK_TERM(rt) \
    do {\
        auto_runtime_t* _rt = (rt);\
        if (!_rt->flag.looping) {\
            longjmp(_rt->checkpoint, 1);\
        }\
    } while(0)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct auto_probe
{
    char                probe[1024];    /**< Probe */
} auto_probe_t;

typedef struct auto_runtime
{
    uv_loop_t           loop;               /**< Event loop */

    struct
    {
        int             looping;            /**< Looping */
        int             gui_ready;          /**< GUI is ready */
    } flag;

    struct
    {
        char*           data;               /**< Script content */
        size_t          size;               /**< Script size */
    } script;

    struct
    {
        char*           compile_path;
        char*           output_path;
        char*           script_path;
    } config;

    jmp_buf             checkpoint;
} auto_runtime_t;

void _init_probe(auto_probe_t* probe);

/**
 * @brief Initialize runtime in lua vm.
 * @param[in] L     Lua VM.
 * @param idx
 * @return          Always 0.
 */
int auto_init_runtime(lua_State* L, int argc, char* argv[]);

/**
 * @brief Get runtime from lua vm.
 * @param[in] L     Lua VM.
 * @return          Runtime instance.
 */
auto_runtime_t* auto_get_runtime(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
