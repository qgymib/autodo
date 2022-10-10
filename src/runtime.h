#ifndef __AUTO_RUNTIME_H__
#define __AUTO_RUNTIME_H__

#include <uv.h>
#include "lua/api.h"
#include "utils/list.h"
#include "utils/map.h"

/**
 * @brief The period of check global looping flag.
 * This is a appropriate in milliseconds
 */
#define AUTO_CHECK_PERIOD   100

#ifdef __cplusplus
extern "C" {
#endif

struct atd_coroutine_impl;
typedef struct atd_coroutine_impl atd_coroutine_impl_t;

struct atd_coroutine_hook
{
    atd_list_node_t         node;
    atd_coroutine_hook_fn   fn;
    void*                   arg;
    atd_coroutine_impl_t*   impl;
};

typedef struct atd_runtime
{
    lua_State*              L;              /**< Lua VM */
    uv_loop_t               loop;           /**< Event loop */
    uv_async_t              notifier;       /**< Event notifier */

    struct
    {
        void*               data;           /**< Script content */
        size_t              size;           /**< Script size */
    } script;

    struct
    {
        char*               script_file;    /**< Full path to run script */
        char*               script_path;    /**< Path to script without file name */
        char*               script_name;    /**< Script name without path */
    } config;

    struct
    {
        atd_map_t           all_table;      /**< All registered coroutine */
        atd_list_t          busy_queue;     /**< Coroutine that ready to schedule */
        atd_list_t          wait_queue;     /**< Coroutine that wait for some events */
    } schedule;

    struct
    {
        char                errbuf[1024];
    } cache;
} atd_runtime_t;

struct atd_coroutine_impl
{
    atd_list_node_t         q_node;         /**< Schedule queue node */
    atd_map_node_t          t_node;         /**< Schedule table node */

    atd_runtime_t*          rt;
    atd_coroutine_t         base;           /**< Base object */

    struct
    {
        int                 ref_key;        /**< Reference key of coroutine in Lua VM */
    } data;

    struct
    {
        atd_list_t          queue;          /**< Schedule hook queue */
        atd_list_node_t*    it;             /**< Global iterator */
    } hook;

    struct
    {
        unsigned            protect : 1;    /**< Run in protected mode */
    } flags;
};

/**
 * @brief Initialize runtime.
 * @param[in] argc  Argument list size.
 * @param[in] argv  Argument list.
 * @return          Always 0.
 */
AUTO_LOCAL int atd_init_runtime(lua_State* L, int argc, char* argv[]);

AUTO_LOCAL atd_runtime_t* auto_get_runtime(lua_State* L);

/**
 * @brief Run scheduler.
 *
 * The scheduler finish when looping flag is clear or all coroutine is done.
 *
 * To yield from current coroutine, use lua_yield() or lua_yieldk().
 *
 * @note All coroutine created by #atd_new_thread() is automatically managed
 *   by scheduler.
 * @param[in] rt    Global runtime.
 * @param[in] L     The thread that host scheduler.
 * @return          Error code.
 */
AUTO_LOCAL int atd_schedule(atd_runtime_t* rt, lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
