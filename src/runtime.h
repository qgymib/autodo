#ifndef __AUTO_RUNTIME_H__
#define __AUTO_RUNTIME_H__

#include <uv.h>
#include <setjmp.h>
#include "lua/api.h"
#include "utils/list.h"
#include "utils/map.h"

/**
 * @brief Terminate script if necessary.
 * @param[in] rt    Runtime.
 */
#define AUTO_CHECK_TERM(rt) \
    do {\
        atd_runtime_t* _rt = (rt);\
        if (!_rt->flag.looping) {\
            longjmp(_rt->check.point, 1);\
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

struct atd_coroutine_impl;
typedef struct atd_coroutine_impl atd_coroutine_impl_t;

struct atd_coroutine_hook
{
    ev_list_node_t          node;
    atd_coroutine_hook_fn   fn;
    void*                   arg;
    atd_coroutine_impl_t*   impl;
};

struct atd_coroutine_impl
{
    ev_list_node_t      q_node;         /**< Schedule queue node */
    ev_map_node_t       t_node;         /**< Schedule table node */

    atd_coroutine_t     base;           /**< Base object */

    struct
    {
        int             ref_key;        /**< Reference key of coroutine in Lua VM */
    } data;

    struct
    {
        ev_list_t       queue;          /**< Schedule hook queue */
        ev_list_node_t* it;             /**< Global iterator */
    } hook;
};

typedef struct atd_runtime
{
    lua_State*          L;              /**< Lua VM */
    uv_loop_t           loop;           /**< Event loop */
    uv_async_t          notifier;       /**< Event notifier */

    struct
    {
        int             looping;        /**< Looping */
    } flag;

    struct
    {
        void*           data;           /**< Script content */
        size_t          size;           /**< Script size */
    } script;

    struct
    {
        char*           script_name;    /**< Script name */
        char*           script_path;    /**< Path to run script */
    } config;

    struct
    {
        ev_map_t        all_table;      /**< All registered coroutine */
        ev_list_t       busy_queue;     /**< Coroutine that ready to schedule */
        ev_list_t       wait_queue;     /**< Coroutine that wait for some events */
    } schedule;

    struct
    {
        char            errbuf[1024];
    } cache;

    struct
    {
        jmp_buf         point;
    } check;
} atd_runtime_t;

extern atd_runtime_t*   atd_rt;

/**
 * @brief Initialize runtime.
 * @param[in] argc  Argument list size.
 * @param[in] argv  Argument list.
 * @return          Always 0.
 */
int atd_init_runtime(int argc, char* argv[]);

/**
 * @brief Exit runtime.
 */
void atd_exit_runtime(void);

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
int atd_schedule(atd_runtime_t* rt, lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
