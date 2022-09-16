#ifndef __AUTODO_H__
#define __AUTODO_H__

#include <stdint.h>

/**
 * @brief Check if the coroutine \p thr is in busy state.
 * @param[in] thr   The coroutine.
 * @return          bool.
 */
#define AUTO_THREAD_IS_BUSY(thr)    ((thr)->status == LUA_TNONE)

/**
 * @brief Check if the coroutine \p thr is in yield state.
 * @param[in] thr   The coroutine.
 * @return          bool.
 */
#define AUTO_THREAD_IS_WAIT(thr)    ((thr)->status == LUA_YIELD)

/**
 * @brief Check if the coroutine \p is dead. That is either finish execution or
 *   error occur.
 * @param[in] thr   The coroutine.
 * @return          bool.
 */
#define AUTO_THREAD_IS_DEAD(thr)    (!AUTO_THREAD_IS_BUSY(thr) && !AUTO_THREAD_IS_WAIT(thr))

/**
 * @brief Check if the coroutine \p thr have error.
 * @param[in] thr   The coroutine.
 * @return          bool.
 */
#define AUTO_THREAD_IS_ERROR(thr)   \
    (\
        (thr)->status != LUA_TNONE &&\
        (thr)->status != LUA_YIELD &&\
        (thr)->status != LUA_OK\
    )

#if defined(_WIN32)
#   if defined(ATD_API_EXPORT)
#       define ATD_API __declspec(dllexport)
#   else
#       define ATD_API __declspec(dllimport)
#   endif
#else
#   define ATD_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef void (*atd_async_fn)(void* arg);
typedef void (*atd_thread_fn)(void* arg);
typedef void (*atd_timer_fn)(void* arg);

/**
 * @brief The list node.
 * This node must put in your struct.
 */
typedef struct atd_list_node_s
{
    struct atd_list_node_s*    p_after;    /**< Pointer to next node */
    struct atd_list_node_s*    p_before;   /**< Pointer to previous node */
} std_list_node_t;

typedef struct atd_list
{
    /**
     * @brief Destroy list.
     * @warning It does not free internal node, so you need to clear the list first.
     * @param[in] thiz      This object.
     */
    void(*destroy)(struct atd_list* thiz);

    /**
     * @brief Insert a node to the head of the list.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in,out] n     Pointer to a new node
     */
    void (*push_front)(struct atd_list* thiz, std_list_node_t* n);

    /**
      * @brief Insert a node to the tail of the list.
      * @warning the node must not exist in any list.
      * @warning MT-UnSafe
      * @param[in] thiz      This object.
      * @param[in,out] n     Pointer to a new node
      */
    void (*push_back)(struct atd_list* thiz, std_list_node_t* n);

    /**
     * @brief Insert a node in front of a given node.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in,out] p     Pointer to a exist node
     * @param[in,out] n     Pointer to a new node
     */
    void (*insert_before)(struct atd_list* thiz, std_list_node_t* p, std_list_node_t* n);

    /**
     * @brief Insert a node right after a given node.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in,out] p     Pointer to a exist node
     * @param[in,out] n     Pointer to a new node
     */
    void (*insert_after)(struct atd_list* thiz, std_list_node_t* p, std_list_node_t* n);

    /**
     * @brief Delete a exist node
     * @warning The node must already in the list.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in,out] n     The node you want to delete
     */
    void (*erase)(struct atd_list* thiz, std_list_node_t* n);

    /**
     * @brief Get the number of nodes in the list.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @return          The number of nodes
     */
    size_t (*size)(struct atd_list* thiz);

    /**
     * @brief Get the first node and remove it from the list.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @return              The first node
     */
    std_list_node_t* (*pop_front)(struct atd_list* thiz);

    /**
     * @brief Get the last node and remove it from the list.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @return              The last node
     */
    std_list_node_t* (*pop_back)(struct atd_list* thiz);

    /**
     * @brief Get the first node.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @return              The first node
     */
    std_list_node_t* (*begin)(struct atd_list* thiz);

    /**
     * @brief Get the last node.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @return              The last node
     */
    std_list_node_t* (*end)(struct atd_list* thiz);

    /**
     * @brief Get next node.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in] node      Current node.
     * @return              The next node
     */
    std_list_node_t* (*next)(struct atd_list* thiz, const std_list_node_t* node);

    /**
     * @brief Get previous node.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in] n         current node
     * @return              previous node
     */
    std_list_node_t* (*prev)(struct atd_list* thiz, const std_list_node_t* n);

    /**
     * @brief Move all elements from \p src into the end of this object.
     * @warning MT-UnSafe
     * @param[in] thiz  This object.
     * @param[in] src   Source list.
     */
    void (*migrate)(struct atd_list* thiz, struct atd_list* src);
} atd_list_t;

typedef struct atd_sem
{
    /**
     * @brief Destroy semaphore.
     * @warning MT-UnSafe
     * @param[in] thiz  This object.
     */
    void (*destroy)(struct atd_sem* thiz);

    /**
     * @brief Wait for signal.
     * @note MT-Safe
     * @param[in] thiz  This object.
     */
    void (*wait)(struct atd_sem* thiz);

    /**
     * @brief Post signal.
     * @note MT-Safe
     * @param[in] thiz  This object.
     */
    void (*post)(struct atd_sem* thiz);
} atd_sem_t;

typedef struct atd_sync
{
    /**
     * @brief Destroy this object.
     * @warning MT-UnSafe
     * @param[in] thiz  This object.
     */
    void (*destroy)(struct atd_sync* thiz);

    /**
     * @brief Wakeup callback.
     * @note MT-Safe
     * @param[in] thiz  This object.
     */
    void (*send)(struct atd_sync* thiz);
} atd_sync_t;

typedef struct atd_timer
{
    /**
     * @brief Destroy timer.
     * @warning MT-UnSafe
     * @param[in] thiz  This object.
     */
    void (*destroy)(struct atd_timer* thiz);

    /**
     * @brief Start timer.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in] timeout   Timeout in milliseconds.
     * @param[in] repeat    If non-zero, the callback fires first after \p timeout
     *   milliseconds and then repeatedly after \p repeat milliseconds.
     * @param[in] fn        Timeout callback.
     * @param[in] arg       User defined argument passed to \p fn.
     */
    void (*start)(struct atd_timer* thiz, uint64_t timeout, uint64_t repeat,
        atd_timer_fn fn, void* arg);

    /**
     * @brief Stop the timer.
     * @warning MT-UnSafe
     * @param[in] thiz  This object.
     */
    void (*stop)(struct atd_timer* thiz);
} atd_timer_t;

typedef struct atd_thread
{
    /**
     * @brief Wait for thread finish and release this object.
     * @note MT-Safe
     * @param[in] thiz  This object.
     */
    void (*join)(struct atd_thread* thiz);
} atd_thread_t;

struct atd_process;
typedef struct atd_process atd_process_t;

/**
 * @brief Process stdio callback.
 * @param[in] process   Process object.
 * @param[in] data      The data to send.
 * @param[in] size      The data size.
 * @param[in] status    IO result.
 * @param[in] arg       User defined argument.
 */
typedef void (*atd_process_stdio_fn)(atd_process_t* process, void* data,
    size_t size, int status, void* arg);

typedef struct atd_process_cfg
{
    const char*             path;       /**< File path. */
    const char*             cwd;        /**< (Optional) Working directory. */
    char**                  args;       /**< Arguments passed to process. */
    char**                  envs;       /**< (Optional) Environments passed to process. */
    atd_process_stdio_fn    stdout_fn;  /**< (Optional) Child stdout callback. */
    atd_process_stdio_fn    stderr_fn;  /**< (Optional) Child stderr callback. */
    void*                   arg;        /**< User defined data passed to \p stdout_fn and \p stderr_fn */
} atd_process_cfg_t;

struct atd_process
{
    /**
     * @brief Kill process.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in] signum    Signal number.
     */
    void (*kill)(struct atd_process* thiz, int signum);

    /**
     * @brief Async send data to child process stdin.
     * @warning MT-UnSafe
     * @param[in] thiz      This object.
     * @param[in] data      The data to send. Do not release it until \p cb is called.
     * @param[in] size      The data size.
     * @return              Error code.
     */
    int (*send_to_stdin)(struct atd_process* thiz, void* data, size_t size,
        atd_process_stdio_fn cb, void* arg);
};

struct atd_coroutine_hook;
typedef struct atd_coroutine_hook atd_coroutine_hook_t;

struct atd_coroutine;
typedef struct atd_coroutine atd_coroutine_t;

typedef void(*atd_coroutine_hook_fn)(atd_coroutine_t* coroutine, void* arg);

struct atd_coroutine
{
    /**
     * @brief The registered coroutine.
     */
    lua_State*  L;

    /**
     * @brief Thread schedule status.
     *
     * The coroutine status define as:
     * + LUA_TNONE:     Busy. The coroutine will be scheduled soon.
     * + LUA_YIELD:     Wait. The coroutine will not be scheduled.
     * + LUA_OK:        Finish. The coroutine will be destroyed soon.
     * + Other value:   Error. The coroutine will be destroyed soon.
     */
    int         status;

    /**
     * @brief The number of returned values.
     */
    int         nresults;

    /**
     * @brief Add schedule hook for this coroutine.
     *
     * A hook will be active every time the coroutine is scheduled.
     *
     * The hook must unregistered when coroutine finish execution or error
     * happen (That is, the #atd_coroutine_t::status is not `LUA_TNONE` or
     * `LUA_YIELD`).
     *
     * @warning MT-UnSafe
     * @note You can not call `lua_yield()` in the callback.
     * @param[in] token Hook token.
     * @param[in] fn    Schedule callback.
     * @param[in] arg   User defined data passed to \p fn.
     */
    atd_coroutine_hook_t* (*hook)(struct atd_coroutine* thiz, atd_coroutine_hook_fn fn, void* arg);

    /**
     * @brief Unregister schedule hook.
     * @warning MT-UnSafe
     * @param[in] thiz  This object.
     * @param[in] token Schedule hook return by #atd_coroutine_t::hook().
     */
    void (*unhook)(struct atd_coroutine* thiz, atd_coroutine_hook_t* token);

    /**
     * @brief Set coroutine schedule state.
     *
     * A simple `lua_yield()` call cannot prevent coroutine from running: it
     * will be scheduled in next loop.
     *
     * To stop the coroutine from scheduling, use this function to set the
     * coroutine to `LUA_YIELD` state.
     *
     * A coroutine in `LUA_YIELD` will not be scheduled until it is set back to
     * `LUA_TNONE` state.
     *
     * @warning MT-UnSafe
     * @param[in] thiz  This object.
     * @param[in] busy  New schedule state. It only can be `LUA_TNONE` or `LUA_YIELD`.
     */
    void (*set_schedule_state)(struct atd_coroutine* thiz, int state);
};

/**
 * @brief Autodo API.
 *
 * To get this api structure, either by #atd_api() or:
 *
 * ```c
 * lua_getglobal(L, "auto");
 * lua_getfield(L, -1, "api");
 * atd_api_t* api = lua_touserdata(-1);
 * ```
 *
 * The lifetime of api is the same as host progress, so you are free to use it
 * any time.
 *
 * @warning For functions have tag `MT-UnSafe`, it means you must call these
 *   functions in the same thread as lua vm host.
 */
typedef struct atd_api_s
{
    /**
     * @brief Returns the current high-resolution real time in nanoseconds.
     *
     * It is relative to an arbitrary time in the past. It is not related to
     * the time of day and therefore not subject to clock drift.
     *
     * @note MT-Safe
     * @return nanoseconds.
     */
    uint64_t (*hrtime)(void);

    /**
     * @brief Create a new list.
     * @note MT-Safe
     * @return  List object.
     */
    atd_list_t* (*new_list)(void);

    /**
     * @brief Create a new semaphore.
     * @note MT-Safe
     * @param[in] value     Initial semaphore value.
     * @return              Semaphore object.
     */
    atd_sem_t* (*new_sem)(unsigned int value);

    /**
     * @brief Create a new native thread.
     * @note MT-Safe
     * @param[in] fn    Thread body.
     * @param[in] arg   User defined data passed to \p fn.
     * @return          Thread object.
     */
    atd_thread_t* (*new_thread)(atd_thread_fn fn, void* arg);

    /**
     * @brief Create a new async object.
     * @note You must release this object before script exit.
     * @warning MT-UnSafe
     * @param[in] fn    Active callback.
     * @param[in] arg   User defined data passed to \p fn.
     * @return          Async object.
     */
    atd_sync_t* (*new_async)(atd_async_fn fn, void* arg);

    /**
     * @brief Create a new timer.
     * @warning MT-UnSafe
     * @return          Timer object.
     */
    atd_timer_t* (*new_timer)(void);

    /**
     * @brief Create a new process.
     * @warning MT-UnSafe
     * @param[in] cfg   Process configuration.
     * @return          Process object.
     */
    atd_process_t* (*new_process)(atd_process_cfg_t* cfg);

    /**
     * @brief Register lua coroutine \p L and let scheduler manage it's life cycle.
     *
     * A new object #atd_coroutine_t mapping to this lua coroutine is created,
     * you can use this object to do some manage operations.
     *
     * You must use this object carefully, as the life cycle is managed by
     * scheduler. To known when the object is destroyed, register schedule
     * callback by #atd_coroutine_t::hook().
     *
     * @note A lua coroutine can only register once.
     * @note This function does not affect lua stack.
     * @warning MT-UnSafe
     * @param[in] L     The coroutine created by `lua_newthread()`.
     * @return          A mapping object.
     */
    atd_coroutine_t* (*register_coroutine)(lua_State* L);

    /**
     * @brief Find mapping coroutine object from lua coroutine \p L.
     * @warning MT-UnSafe
     * @param[in] L     The coroutine created by `lua_newthread()`.
     * @return          The mapping coroutine object, or `NULL` if not found.
     */
    atd_coroutine_t* (*find_coroutine)(lua_State* L);
} atd_api_t;

/**
 * @brief Get API.
 *
 * This function call is extremely cheap, it returns the address of a
 * predefined structure. So it should be ok not to cache the result.
 *
 * @return API.
 */
ATD_API atd_api_t* atd_api(void);

#ifdef __cplusplus
}
#endif

#endif
