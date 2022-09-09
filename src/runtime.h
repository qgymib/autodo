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
        auto_runtime_t* _rt = (rt);\
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

typedef struct auto_thread
{
    ev_list_node_t      q_node;
    ev_map_node_t       t_node;

    lua_State*          co;                 /**< Coroutine, also as find key */

    struct
    {
        int             ref_key;            /**< Reference key of coroutine */

        /**
         * @brief Thread status
         * + LUA_YIELD: Wait status
         * + LUA_OK:    Busy status
         */
        int             status;
    } data;
} auto_thread_t;

typedef struct auto_runtime
{
    uv_loop_t           loop;               /**< Event loop */
    uv_async_t          notifier;           /**< Event notifier */

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
        ev_map_t        all_table;          /**< All registered coroutine */
        ev_list_t       busy_queue;         /**< Coroutine that ready to schedule */
        ev_list_t       wait_queue;         /**< Coroutine that wait for some events */
    } schedule;

    struct
    {
        char            errbuf[1024];
    } cache;

    struct
    {
        jmp_buf         point;
    } check;
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

/**
 * @brief Create a new lua coroutine and save into schedule queue.
 *
 * The coroutine will be scheduled as soon as possible.
 *
 * @param[in] L     Lua VM.
 * @return          A new coroutine.
 */
auto_thread_t* auto_new_thread(auto_runtime_t* rt, lua_State* L);

/**
 * @brief Destroy a coroutine.
 * @param[in] rt    Global runtime.
 * @param[in] thr   Coroutine
 */
void auto_release_thread(auto_runtime_t* rt, auto_thread_t* thr);

/**
 * @brief Find coroutine by lua thread.
 * @param[in] rt    Global runtime.
 * @param[in] L     Lua thread.
 * @return          Coroutine.
 */
auto_thread_t* auto_find_thread(auto_runtime_t* rt, lua_State* L);

/**
 * @brief Set \p thr as busy status.
 * @param[in] rt    Global runtime.
 * @param[in] thr   Coroutine.
 */
void auto_set_thread_as_busy(auto_runtime_t* rt, auto_thread_t* thr);

/**
 * @brief Set \p thr as wait status.
 * @param[in] rt    Global runtime.
 * @param[in] thr   Coroutine.
 */
void auto_set_thread_as_wait(auto_runtime_t* rt, auto_thread_t* thr);

/**
 * @brief Run scheduler
 * @param[in] rt    Global runtime.
 * @param[in] L     The thread that host scheduler.
 * @return          Error code.
 */
int auto_schedule(auto_runtime_t* rt, lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
