#ifndef __AUTO_DO_H__
#define __AUTO_DO_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define AUTO_VERSION_MAJOR  0
#define AUTO_VERSION_MINOR  0
#define AUTO_VERSION_PATCH  1

/**
 * @brief Shortcut for #auto_get_api().
 *
 * It is recommend to cache this result to avoid multiple duplicated lookup.
 *
 * @return      The API struct.
 */
#define AUTO_GET_API()  \
    (auto_get_api(AUTO_VERSION_MAJOR, AUTO_VERSION_MINOR, AUTO_VERSION_PATCH))

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
#   if defined(AUTO_API_EXPORT)
#       define AUTO_EXPORT_API  __declspec(dllexport)
#   else
#       define AUTO_EXPORT_API  __declspec(dllimport)
#   endif
#else
#   define AUTO_EXPORT_API
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

/**
 * @brief Double Linked List
 */
typedef struct atd_list_s
{
    std_list_node_t*        head;       /**< Pointer to HEAD node */
    std_list_node_t*        tail;       /**< Pointer to TAIL node */
    size_t                  size;       /**< The number of total nodes */
} atd_list_t;

/**
 * @brief ev_map_low node
 * @see EV_MAP_LOW_NODE_INIT
 */
typedef struct atd_map_node
{
    struct atd_map_node* __rb_parent_color;  /**< parent node | color */
    struct atd_map_node* rb_right;           /**< right node */
    struct atd_map_node* rb_left;            /**< left node */
} atd_map_node_t;

/**
 * @brief Compare function.
 * @param[in] key1  The key in the map
 * @param[in] key2  The key user given
 * @param[in] arg   User defined argument
 * @return          -1 if key1 < key2. 1 if key1 > key2. 0 if key1 == key2.
 */
typedef int(*atd_map_cmp_fn)(const atd_map_node_t* key1, const atd_map_node_t* key2, void* arg);

/**
 * @brief Map implemented as red-black tree
 * @see EV_MAP_INIT
 */
typedef struct atd_map_s
{
    atd_map_node_t*     rb_root;    /**< root node */

    struct
    {
        atd_map_cmp_fn  cmp;        /**< Pointer to compare function */
        void*           arg;        /**< User defined argument, which will passed to compare function */
    } cmp;                          /**< Compare function data */

    size_t              size;       /**< The number of nodes */
} atd_map_t;

struct atd_sem_s;
typedef struct atd_sem_s atd_sem_t;

struct atd_sync_s;
typedef struct atd_sync_s atd_sync_t;

struct atd_timer_s;
typedef struct atd_timer_s atd_timer_t;

struct atd_thread_s;
typedef struct atd_thread_s atd_thread_t;

struct atd_process_s;
typedef struct atd_process_s atd_process_t;

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
};

/**
 * @brief Autodo API.
 *
 * To get this api structure, either by #AUTO_GET_API() or by following c code:
 *
 * ```c
 * lua_getglobal(L, "auto");
 * lua_getfield(L, -1, "get_c_api");
 * lua_pushnumber(L, AUTO_VERSION_MAJOR);
 * lua_pushnumber(L, AUTO_VERSION_MINOR);
 * lua_pushnumber(L, AUTO_VERSION_PATCH);
 * lua_call(L, 3, 1);
 * const auto_api_t* api = lua_touserdata(-1);
 * ```
 *
 * The lifetime of api is the same as host progress, so you are free to use it
 * any time.
 *
 * @warning For functions have tag `MT-UnSafe`, it means you must call these
 *   functions in the same thread as lua vm host.
 */
typedef struct auto_api_s {
    struct
    {
        /**
         * @brief Create a new list.
         * @note MT-Safe
         * @return  List object.
         */
        void (*init)(atd_list_t* handler);

        /**
         * @brief Insert a node to the head of the list.
         * @warning the node must not exist in any list.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @param[in,out] n     Pointer to a new node
         */
        void (*push_front)(atd_list_t* self, std_list_node_t* n);

        /**
          * @brief Insert a node to the tail of the list.
          * @warning the node must not exist in any list.
          * @warning MT-UnSafe
          * @param[in] self      This object.
          * @param[in,out] n     Pointer to a new node
          */
        void (*push_back)(atd_list_t* self, std_list_node_t* n);

        /**
         * @brief Insert a node in front of a given node.
         * @warning the node must not exist in any list.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @param[in,out] p     Pointer to a exist node
         * @param[in,out] n     Pointer to a new node
         */
        void (*insert_before)(atd_list_t* self, std_list_node_t* p, std_list_node_t* n);

        /**
         * @brief Insert a node right after a given node.
         * @warning the node must not exist in any list.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @param[in,out] p     Pointer to a exist node
         * @param[in,out] n     Pointer to a new node
         */
        void (*insert_after)(atd_list_t* self, std_list_node_t* p, std_list_node_t* n);

        /**
         * @brief Delete a exist node
         * @warning The node must already in the list.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @param[in,out] n     The node you want to delete
         */
        void (*erase)(atd_list_t* self, std_list_node_t* n);

        /**
         * @brief Get the number of nodes in the list.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @return          The number of nodes
         */
        size_t (*size)(const atd_list_t* handler);

        /**
         * @brief Get the first node and remove it from the list.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @return              The first node
         */
        std_list_node_t* (*pop_front)(atd_list_t* self);

        /**
         * @brief Get the last node and remove it from the list.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @return              The last node
         */
        std_list_node_t* (*pop_back)(atd_list_t* self);

        /**
         * @brief Get the first node.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @return              The first node
         */
        std_list_node_t* (*begin)(const atd_list_t* self);

        /**
         * @brief Get the last node.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @return              The last node
         */
        std_list_node_t* (*end)(const atd_list_t* self);

        /**
         * @brief Get next node.
         * @warning MT-UnSafe
         * @param[in] node      Current node.
         * @return              The next node
         */
        std_list_node_t* (*next)(const std_list_node_t* node);

        /**
         * @brief Get previous node.
         * @warning MT-UnSafe
         * @param[in] node      current node
         * @return              previous node
         */
        std_list_node_t* (*prev)(const std_list_node_t* node);

        /**
         * @brief Move all elements from \p src into the end of \p dst.
         * @warning MT-UnSafe
         * @param[in] dst   Destination list.
         * @param[in] src   Source list.
         */
        void (*migrate)(atd_list_t* dst, atd_list_t* src);
    } list;

    struct
    {
        /**
         * @brief Initialize the map referenced by handler.
         * @param[out] self     The pointer to the map
         * @param[in] cmp       The compare function. Must not NULL
         * @param[in] arg       User defined argument. Can be anything
         */
        void (*init)(atd_map_t* self, atd_map_cmp_fn cmp, void *arg);

        /**
         * @brief Insert the node into map.
         * @warning MT-UnSafe
         * @warning the node must not exist in any map.
         * @param[in] self      The pointer to the map
         * @param[in] node      The node
         * @return              NULL if success, otherwise return the original node.
         */
        atd_map_node_t* (*insert)(atd_map_t* self, atd_map_node_t* node);

        /**
         * @brief Replace a existing data with \p node.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @param[in] node  The node to insert.
         * @return          NULL if no existing data, otherwise return the replaced node.
         */
        atd_map_node_t* (*replace)(atd_map_t* self, atd_map_node_t* node);

        /**
         * @brief Delete the node from the map.
         * @warning The node must already in the map.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @param[in] node  The node
         */
        void (*erase)(atd_map_t* self, atd_map_node_t* node);

        /**
         * @brief Get the number of nodes in the map.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @return          The number of nodes
         */
        size_t (*size)(const atd_map_t* self);

        /**
         * @brief Finds element with specific key.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @param[in] key   The key
         * @return          An iterator point to the found node
         */
        atd_map_node_t* (*find)(const atd_map_t* self, const atd_map_node_t* key);

        /**
         * @brief Returns an iterator to the first element not less than the given key.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @param[in] key   The key
         * @return          An iterator point to the found node
         */
        atd_map_node_t* (*find_lower)(const atd_map_t* self, const atd_map_node_t* key);

        /**
         * @brief Returns an iterator to the first element greater than the given key.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @param[in] key   The key
         * @return          An iterator point to the found node
         */
        atd_map_node_t* (*find_upper)(const atd_map_t* self, const atd_map_node_t* key);

        /**
         * @brief Returns an iterator to the beginning.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @return          An iterator
         */
        atd_map_node_t* (*begin)(const atd_map_t* self);

        /**
         * @brief Returns an iterator to the end.
         * @warning MT-UnSafe
         * @param[in] self  The pointer to the map.
         * @return          An iterator
         */
        atd_map_node_t* (*end)(const atd_map_t* self);

        /**
         * @brief Get an iterator next to the given one.
         * @warning MT-UnSafe
         * @param[in] node  Current iterator
         * @return          Next iterator
         */
        atd_map_node_t* (*next)(const atd_map_node_t* node);

        /**
         * @brief Get an iterator before the given one.
         * @warning MT-UnSafe
         * @param[in] node  Current iterator
         * @return          Previous iterator
         */
        atd_map_node_t* (*prev)(const atd_map_node_t* node);
    } map;

    struct
    {
        /**
         * @brief Create a new semaphore.
         * @note MT-Safe
         * @param[in] value     Initial semaphore value.
         * @return              Semaphore object.
         */
        atd_sem_t* (*create)(unsigned int value);

        /**
         * @brief Destroy semaphore.
         * @warning MT-UnSafe
         * @param[in] self  This object.
         */
        void (*destroy)(atd_sem_t* self);

        /**
         * @brief Wait for signal.
         * @note MT-Safe
         * @param[in] self  This object.
         */
        void (*wait)(atd_sem_t* self);

        /**
         * @brief Post signal.
         * @note MT-Safe
         * @param[in] self  This object.
         */
        void (*post)(atd_sem_t* self);
    } sem;

    struct
    {
        /**
         * @brief Create a new native thread.
         * @note MT-Safe
         * @param[in] fn    Thread body.
         * @param[in] arg   User defined data passed to \p fn.
         * @return          Thread object.
         */
        atd_thread_t* (*create)(atd_thread_fn fn, void *arg);

        /**
         * @brief Wait for thread finish and release this object.
         * @note MT-Safe
         * @param[in] self  This object.
         */
        void (*join)(atd_thread_t* self);

        /**
         * @brief Causes the calling thread to sleep for \p ms milliseconds.
         * @param[in] ms    Milliseconds.
         */
        void (*sleep)(uint32_t ms);
    } thread;

    struct
    {
        /**
         * @brief Create a new async object.
         * @note You must release this object before script exit.
         * @warning MT-UnSafe
         * @param[in] fn    Active callback.
         * @param[in] arg   User defined data passed to \p fn.
         * @return          Async object.
         */
        atd_sync_t* (*create)(atd_async_fn fn, void *arg);

        /**
         * @brief Destroy this object.
         * @warning MT-UnSafe
         * @param[in] self  This object.
         */
        void (*destroy)(atd_sync_t* self);

        /**
         * @brief Wakeup callback.
         * @note MT-Safe
         * @param[in] self  This object.
         */
        void (*send)(atd_sync_t* self);
    } async;

    struct
    {
        /**
         * @brief Create a new timer.
         * @warning MT-UnSafe
         * @return          Timer object.
         */
        atd_timer_t* (*create)(void);

        /**
         * @brief Destroy timer.
         * @warning MT-UnSafe
         * @param[in] self  This object.
         */
        void (*destroy)(atd_timer_t* self);

        /**
         * @brief Start timer.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @param[in] timeout   Timeout in milliseconds.
         * @param[in] repeat    If non-zero, the callback fires first after \p timeout
         *   milliseconds and then repeatedly after \p repeat milliseconds.
         * @param[in] fn        Timeout callback.
         * @param[in] arg       User defined argument passed to \p fn.
         */
        void (*start)(atd_timer_t* self, uint64_t timeout, uint64_t repeat,
            atd_timer_fn fn, void* arg);

        /**
         * @brief Stop the timer.
         * @warning MT-UnSafe
         * @param[in] self  This object.
         */
        void (*stop)(atd_timer_t* self);
    } timer;

    struct
    {
        /**
         * @brief Create a new process.
         * You must release process resource by #auto_api_t::process::kill().
         * @warning MT-UnSafe
         * @param[in] cfg   Process configuration.
         * @return          Process object.
         */
        atd_process_t* (*create)(atd_process_cfg_t *cfg);

        /**
         * @brief Kill process.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @param[in] signum    Signal number.
         */
        void (*kill)(atd_process_t* self, int signum);

        /**
         * @brief Async send data to child process stdin.
         * @warning MT-UnSafe
         * @param[in] self      This object.
         * @param[in] data      The data to send. Do not release it until \p cb is called.
         * @param[in] size      The data size.
         * @return              Error code.
         */
        int (*send_to_stdin)(atd_process_t* self, void* data, size_t size,
            atd_process_stdio_fn cb, void* arg);
    } process;

    struct
    {
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
        atd_coroutine_t* (*host)(lua_State *L);

        /**
         * @brief Find mapping coroutine object from lua coroutine \p L.
         * @warning MT-UnSafe
         * @param[in] L     The coroutine created by `lua_newthread()`.
         * @return          The mapping coroutine object, or `NULL` if not found.
         */
        atd_coroutine_t* (*find)(lua_State *L);

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
        atd_coroutine_hook_t* (*hook)(atd_coroutine_t* self, atd_coroutine_hook_fn fn, void* arg);

        /**
         * @brief Unregister schedule hook.
         * @warning MT-UnSafe
         * @param[in] self  This object.
         * @param[in] token Schedule hook return by #atd_coroutine_t::hook().
         */
        void (*unhook)(atd_coroutine_t* self, atd_coroutine_hook_t* token);

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
         * @param[in] self  This object.
         * @param[in] busy  New schedule state. It only can be `LUA_TNONE` or `LUA_YIELD`.
         */
        void (*set_state)(atd_coroutine_t* self, int state);
    } coroutine;

    struct
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
    } misc;
} auto_api_t;

/**
 * @brief Get API by version.
 *
 * Due to API is exported by struct, it is extremely easy to cause ABI unstable
 * (as the layout of struct is likely to change).
 *
 * To ensure library developer can have relatively ABI, the API struct is
 * versioned: developer will get matching API struct by the header version.
 *
 * @see #AUTO_GET_API()
 * @param[in] major     Major version.
 * @param[in] minor     Minor version.
 * @param[in] patch     Patch version.
 * @return              Required API struct.
 */
AUTO_EXPORT_API const auto_api_t* auto_get_api(unsigned major, unsigned minor, unsigned patch);

#ifdef __cplusplus
}
#endif

#endif
