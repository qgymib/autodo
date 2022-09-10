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

#define AUTO_THREAD_IS_BUSY(thr)    ((thr)->data.sch_status == LUA_TNONE)
#define AUTO_THREAD_IS_WAIT(thr)    ((thr)->data.sch_status == LUA_YIELD)
#define AUTO_THREAD_IS_DONE(thr)    (!AUTO_THREAD_IS_BUSY(thr) && !AUTO_THREAD_IS_WAIT(thr))
#define AUTO_THREAD_IS_ERROR(thr)   \
    (\
        (thr)->data.sch_status != LUA_TNONE &&\
        (thr)->data.sch_status != LUA_YIELD &&\
        (thr)->data.sch_status != LUA_OK\
    )

#ifdef __cplusplus
extern "C" {
#endif

struct atd_thread;
typedef struct atd_thread atd_thread_t;

typedef void (*atd_thread_hook_fn)(atd_thread_t* thr, void* data);

typedef struct
{
    ev_list_node_t      node;
    atd_thread_hook_fn  fn;
    void*               data;
} atd_thread_hook_t;

struct atd_thread
{
    ev_list_node_t      q_node;         /**< Schedule queue node */
    ev_map_node_t       t_node;         /**< Schedule table node */

    lua_State*          co;             /**< Coroutine, also as find key */

    struct
    {
        int             n_ret;          /**< The number of return value. */

        /**
         * @brief Thread schedule status.
         * + LUA_TNONE:     Busy (busy_queue)
         * + LUA_YIELD:     Wait (wait_queue)
         * + LUA_OK:        Done (wait_queue)
         * + Other value:   Error (wait_queue)
         */
        int             sch_status;

        int             ref_key;        /**< Reference key of coroutine in Lua VM */

        /**
         * @brief One time hook queue for state change.
         * @see atd_thread_hook_t
         */
        ev_list_t       hook;
    } data;
};

typedef struct atd_runtime
{
    uv_loop_t           loop;           /**< Event loop */
    uv_async_t          notifier;       /**< Event notifier */

    struct
    {
        int             looping;        /**< Looping */
        int             gui_ready;      /**< GUI is ready */
    } flag;

    struct
    {
        void*           data;           /**< Script content */
        size_t          size;           /**< Script size */
    } script;

    struct
    {
        char*           compile_path;   /**< Path to script that need compile */
        char*           output_path;    /**< Path to compiled output file */
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

/**
 * @brief Initialize runtime in lua vm.
 * @param[in] L     Lua VM.
 * @param idx
 * @return          Always 0.
 */
int atd_init_runtime(lua_State* L, int argc, char* argv[]);

/**
 * @brief Get runtime from lua vm.
 * @param[in] L     Lua VM.
 * @return          Runtime instance.
 */
atd_runtime_t* atd_get_runtime(lua_State* L);

/**
 * @brief Create a new lua coroutine and save into schedule queue.
 *
 * The coroutine will be scheduled as soon as possible.
 *
 * @param[in] L     Lua VM.
 * @return          A new coroutine.
 */
atd_thread_t* atd_new_thread(atd_runtime_t* rt, lua_State* L);

/**
 * @brief Add one time hook for coroutine \p thr.
 *
 * The hook will be triggered after the coroutine is resumed, that is after the
 * coroutine yield / finish / error.
 *
 * @param[in] token Hook token.
 * @param[in] thr   Target coroutine to hook.
 * @param[in] fn    Callback function when state change.
 * @param[in] arg   User defined arguments passed to \p fn.
 */
void atd_thread_hook(atd_thread_hook_t* token, atd_thread_t* thr,
    atd_thread_hook_fn fn, void* arg);

/**
 * @brief Link runtime as uservalue 1 for value at \p idx.
 *
 * It is necessary to do so. If not linked, lua might release global runtime
 * before other resources, leading to invalid access during resource release
 * process.
 *
 * @param[in] L     Lua VM.
 * @param[in] idx   Value index.
 * @return          Always 0.
 */
int atd_runtime_link(lua_State* L, int idx);

/**
 * @brief Find coroutine by lua thread.
 * @param[in] rt    Global runtime.
 * @param[in] L     Lua thread.
 * @return          Coroutine.
 */
atd_thread_t* atd_find_thread(atd_runtime_t* rt, lua_State* L);

/**
 * @brief Set \p thr to \p state.
 * @param[in] rt    Global runtime.
 * @param[in] thr   Coroutine.
 * @param[in] state New state.
 */
void atd_set_thread_state(atd_runtime_t* rt, atd_thread_t* thr, int state);

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
