#ifndef __AUTO_RUNTIME_H__
#define __AUTO_RUNTIME_H__

#include <uv.h>
#include <setjmp.h>
#include "lua/api.h"

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

/**
 * @brief The period of check global looping flag.
 * This is a appropriate in milliseconds
 */
#define AUTO_CHECK_PERIOD   100

#ifdef __cplusplus
extern "C" {
#endif

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
        void*           data;               /**< Script content */
        size_t          size;               /**< Script size */
    } script;

    struct
    {
        char*           compile_path;
        char*           output_path;
        char*           script_path;
    } config;

    struct
    {
        char            errbuf[1024];
    } cache;

    jmp_buf             checkpoint;
} auto_runtime_t;

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
