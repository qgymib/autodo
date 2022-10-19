#ifndef __AUTO_DO_H__
#define __AUTO_DO_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define AUTO_VERSION_MAJOR  0
#define AUTO_VERSION_MINOR  0
#define AUTO_VERSION_PATCH  1

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

/**
 * @brief Declare as public interface.
 */
#if defined(_WIN32)
#   if defined(AUTO_API_EXPORT)
#       define AUTO_PUBLIC  __declspec(dllexport)
#   else
#       define AUTO_PUBLIC  __declspec(dllimport)
#   endif
#else
#   define AUTO_PUBLIC
#endif

/**
 * @brief Force export symbol.
 */
#if defined(_WIN32)
#   define AUTO_EXPORT  __declspec(dllexport)
#else
#   define AUTO_EXPORT
#endif

/**
 * @brief Declare as internal interface.
 *
 * It is recommend to declare every internal function and variable as
 * #AUTO_LOCAL to avoid implicit conflict.
 */
#if defined(_WIN32)
#   define AUTO_LOCAL
#else
#   define AUTO_LOCAL   __attribute__ ((visibility ("hidden")))
#endif

/**
 * @brief cast a member of a structure out to the containing structure.
 */
#if !defined(container_of)
#if defined(__GNUC__) || defined(__clang__)
#   define container_of(ptr, type, member)   \
        ({ \
            const typeof(((type *)0)->member)*__mptr = (ptr); \
            (type *)((char *)__mptr - offsetof(type, member)); \
        })
#else
#   define container_of(ptr, type, member)   \
        ((type *) ((char *) (ptr) - offsetof(type, member)))
#endif
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
} atd_list_node_t;

/**
 * @brief Double Linked List
 */
typedef struct atd_list_s
{
    atd_list_node_t*        head;       /**< Pointer to HEAD node */
    atd_list_node_t*        tail;       /**< Pointer to TAIL node */
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
typedef int(*atd_map_cmp_fn)(const atd_map_node_t* key1,
    const atd_map_node_t* key2, void* arg);

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
 * @brief Memory API.
 *
 * ```lua
 * auto.c_api_memory
 * ```
 *
 * or
 *
 * ```c
 * lua_getglobal(L, "auto");
 * lua_getfield(L, -1, "c_api_memory");
 * auto_api_memory_t* api = lua_touserdata(L, -1);
 * lua_pop(L, 2);
 * ```
 */
typedef struct auto_api_memory_s
{
    /**
     * @brief The same as [malloc(3)](https://man7.org/linux/man-pages/man3/free.3.html).
     */
    void* (*malloc)(size_t size);

    /**
     * @brief The same as [free(3p)](https://man7.org/linux/man-pages/man3/free.3p.html).
     */
    void (*free)(void* ptr);

    /**
     * @brief The same as [calloc(3p)](https://man7.org/linux/man-pages/man3/calloc.3p.html).
     */
    void* (*calloc)(size_t nmemb, size_t size);

    /**
     * @brief The same as [realloc(3p)](https://man7.org/linux/man-pages/man3/realloc.3p.html).
     */
    void* (*realloc)(void *ptr, size_t size);
} auto_api_memory_t;

/**
 * @brief List API.
 *
 * ```lua
 * auto.c_api_list
 * ```
 */
typedef struct auto_api_list_s
{
    /**
     * @brief Create a new list.
     * @warning MT-UnSafe
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
    void (*push_front)(atd_list_t* self, atd_list_node_t* n);

    /**
      * @brief Insert a node to the tail of the list.
      * @warning the node must not exist in any list.
      * @warning MT-UnSafe
      * @param[in] self      This object.
      * @param[in,out] n     Pointer to a new node
      */
    void (*push_back)(atd_list_t* self, atd_list_node_t* n);

    /**
     * @brief Insert a node in front of a given node.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @param[in,out] p     Pointer to a exist node
     * @param[in,out] n     Pointer to a new node
     */
    void (*insert_before)(atd_list_t* self, atd_list_node_t* p, atd_list_node_t* n);

    /**
     * @brief Insert a node right after a given node.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @param[in,out] p     Pointer to a exist node
     * @param[in,out] n     Pointer to a new node
     */
    void (*insert_after)(atd_list_t* self, atd_list_node_t* p, atd_list_node_t* n);

    /**
     * @brief Delete a exist node
     * @warning The node must already in the list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @param[in,out] n     The node you want to delete
     */
    void (*erase)(atd_list_t* self, atd_list_node_t* n);

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
    atd_list_node_t* (*pop_front)(atd_list_t* self);

    /**
     * @brief Get the last node and remove it from the list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return              The last node
     */
    atd_list_node_t* (*pop_back)(atd_list_t* self);

    /**
     * @brief Get the first node.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return              The first node
     */
    atd_list_node_t* (*begin)(const atd_list_t* self);

    /**
     * @brief Get the last node.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return              The last node
     */
    atd_list_node_t* (*end)(const atd_list_t* self);

    /**
     * @brief Get next node.
     * @warning MT-UnSafe
     * @param[in] node      Current node.
     * @return              The next node
     */
    atd_list_node_t* (*next)(const atd_list_node_t* node);

    /**
     * @brief Get previous node.
     * @warning MT-UnSafe
     * @param[in] node      current node
     * @return              previous node
     */
    atd_list_node_t* (*prev)(const atd_list_node_t* node);

    /**
     * @brief Move all elements from \p src into the end of \p dst.
     * @warning MT-UnSafe
     * @param[in] dst   Destination list.
     * @param[in] src   Source list.
     */
    void (*migrate)(atd_list_t* dst, atd_list_t* src);
} auto_api_list_t;

/**
 * @brief Map API.
 *
 * ```lua
 * auto.c_api_map
 * ```
 */
typedef struct auto_api_map_s
{
    /**
     * @brief Initialize the map referenced by handler.
     * @warning MT-UnSafes
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
} auto_api_map_t;

/**
 * @brief Misc API.
 *
 * ```lua
 * auto.c_api_misc
 * ```
 */
typedef struct auto_api_misc_s
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
     * @brief Find binary data.
     * @note MT-Safe
     * @param[in] data  Binary data to find.
     * @param[in] size  Binary data size in bytes.
     * @param[in] key   The needle data.
     * @param[in] len   The needle data length in bytes.
     * @return          The position of result data.
     */
    ssize_t (*search)(const void* data, size_t size, const void* key, size_t len);
} auto_api_misc_t;

/**
 * @brief Misc API.
 *
 * ```lua
 * auto.c_api_sem
 * ```
 */
typedef struct auto_api_sem_s
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
} auto_api_sem_t;

/**
 * @brief Misc API.
 *
 * ```lua
 * auto.c_api_thread
 * ```
 *
 * @note Due do user script is able to stop at any time, you have to care about
 *   how to exit thread when out of your expect.
 */
typedef struct auto_api_thread_s
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
     * @warning MT-UnSafe
     * @param[in] self  This object.
     */
    void (*join)(atd_thread_t* self);

    /**
     * @brief Causes the calling thread to sleep for \p ms milliseconds.
     * @note MT-Safe
     * @param[in] ms    Milliseconds.
     */
    void (*sleep)(uint32_t ms);
} auto_api_thread_t;

/**
 * @brief Coroutine API.
 *
 * ```lua
 * auto.c_api_coroutine
 * ```
 */
typedef struct auto_api_coroutine_s
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
    atd_coroutine_t* (*find)(lua_State* L);

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
} auto_api_coroutine_t;

/**
 * @brief Timer API.
 *
 * ```lua
 * auto.c_api_timer
 * ```
 */
typedef struct auto_api_timer_s
{
    /**
     * @brief Create a new timer.
     * @warning MT-UnSafe
     * @return          Timer object.
     */
    atd_timer_t* (*create)(lua_State* L);

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
} auto_api_timer_t;

/**
 * @brief Async API.
 *
 * ```lua
 * auto.c_api_async
 * ```
 */
typedef struct auto_api_async_s
{
    /**
     * @brief Create a new async object.
     * @note You must release this object before script exit.
     * @warning MT-UnSafe
     * @param[in] fn    Active callback.
     * @param[in] arg   User defined data passed to \p fn.
     * @return          Async object.
     */
    atd_sync_t* (*create)(lua_State* L, atd_async_fn fn, void *arg);

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
} auto_api_async_t;

/**
 * @brief Int64 API.
 *
 * ```lua
 * auto.c_api_int64
 * ```
 */
typedef struct auto_api_int64_s
{
    /**
     * @brief Push a signed int64_t integer onto top of stack.
     * @param[in] L         Lua VM.
     * @param[in] value     Integer value.
     * @return              Always 1.
     */
    int (*push_value)(lua_State *L, int64_t value);

    /**
     * @brief Get int64_t integer
     * @param[in] L         Lua VM.
     * @param[in] idx       Stack index.
     * @param[out] value    Integer value.
     * @return              bool.
     */
    int (*get_value)(lua_State *L, int idx, int64_t* value);
} auto_api_int64_t;

/**
 * @brief API.
 *
 * ```lua
 * auto.c_api
 * ```
 */
typedef struct auto_api_s
{
    const auto_api_memory_t*    memory;
    const auto_api_list_t*      list;
    const auto_api_map_t*       map;
    const auto_api_sem_t*       sem;
    const auto_api_thread_t*    thread;
    const auto_api_timer_t*     timer;
    const auto_api_async_t*     async;
    const auto_api_coroutine_t* coroutine;
    const auto_api_int64_t*     int64;
    const auto_api_misc_t*      misc;
} auto_api_t;

#ifdef __cplusplus
}
#endif

#endif
