#ifndef __AUTO_DO_H__
#define __AUTO_DO_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define AUTO_VERSION_MAJOR  0
#define AUTO_VERSION_MINOR  0
#define AUTO_VERSION_PATCH  1

/**
 * @brief Declare as public interface.
 */
#if defined(_WIN32)
#   if defined(AUTO_API_EXPORT)
#       define AUTO_PUBLIC  __declspec(dllexport)
#   else
#       define AUTO_PUBLIC  __declspec(dllimport)
#   endif
#elif __GNUC__ >= 4
#   define AUTO_PUBLIC  __attribute__ ((visibility ("default")))
#else
#   define AUTO_PUBLIC
#endif

/**
 * @brief Force export symbol.
 */
#if defined(_WIN32)
#   define AUTO_EXPORT  __declspec(dllexport)
#elif __GNUC__ >= 4
#   define AUTO_EXPORT  __attribute__ ((visibility ("default")))
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
#elif __GNUC__ >= 4
#   define AUTO_LOCAL   __attribute__ ((visibility ("hidden")))
#else
#   define AUTO_LOCAL
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

struct lua_State;

typedef void (*auto_notify_fn)(void* arg);
typedef void (*auto_thread_fn)(void* arg);
typedef void (*auto_timer_fn)(void* arg);

/**
 * @brief The list node.
 * This node must put in your struct.
 */
typedef struct auto_list_node_s
{
    struct auto_list_node_s*   p_after;    /**< Pointer to next node */
    struct auto_list_node_s*   p_before;   /**< Pointer to previous node */
} auto_list_node_t;

/**
 * @brief Double Linked List
 */
typedef struct auto_list_s
{
    auto_list_node_t*       head;       /**< Pointer to HEAD node */
    auto_list_node_t*       tail;       /**< Pointer to TAIL node */
    size_t                  size;       /**< The number of total nodes */
} auto_list_t;

/**
 * @brief ev_map_low node
 * @see EV_MAP_LOW_NODE_INIT
 */
typedef struct auto_map_node
{
    struct auto_map_node* __rb_parent_color;  /**< parent node | color */
    struct auto_map_node* rb_right;           /**< right node */
    struct auto_map_node* rb_left;            /**< left node */
} auto_map_node_t;

/**
 * @brief Compare function.
 * @param[in] key1  The key in the map
 * @param[in] key2  The key user given
 * @param[in] arg   User defined argument
 * @return          -1 if key1 < key2. 1 if key1 > key2. 0 if key1 == key2.
 */
typedef int (*auto_map_cmp_fn)(const auto_map_node_t* key1,
    const auto_map_node_t* key2, void* arg);

/**
 * @brief Map implemented as red-black tree
 * @see EV_MAP_INIT
 */
typedef struct auto_map_s
{
    auto_map_node_t*     rb_root;    /**< root node */

    struct
    {
        auto_map_cmp_fn  cmp;        /**< Pointer to compare function */
        void*           arg;        /**< User defined argument, which will passed to compare function */
    } cmp;                          /**< Compare function data */

    size_t              size;       /**< The number of nodes */
} auto_map_t;

struct auto_sem_s;
typedef struct auto_sem_s auto_sem_t;

struct auto_notify_s;
typedef struct auto_notify_s auto_notify_t;

struct auto_timer_s;
typedef struct auto_timer_s auto_timer_t;

struct auto_thread_s;
typedef struct auto_thread_s auto_thread_t;

struct auto_coroutine_hook;
typedef struct auto_coroutine_hook auto_coroutine_hook_t;

struct auto_coroutine;
typedef struct auto_coroutine auto_coroutine_t;

typedef void(*auto_coroutine_hook_fn)(auto_coroutine_t* coroutine, void* arg);

struct auto_coroutine
{
    /**
     * @brief The registered coroutine.
     */
    struct lua_State*   L;

    /**
     * @brief Thread schedule status.
     * Bit-OR of #auto_coroutine_state_t.
     */
    int                 status;

    /**
     * @brief The number of returned values.
     */
    int                 nresults;
};

/**
 * @brief Memory API.
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
 */
typedef struct auto_api_list_s
{
    /**
     * @brief Create a new list.
     * @warning MT-UnSafe
     * @return  List object.
     */
    void (*init)(auto_list_t* handler);

    /**
     * @brief Insert a node to the head of the list.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @param[in,out] n     Pointer to a new node
     */
    void (*push_front)(auto_list_t* self, auto_list_node_t* n);

    /**
      * @brief Insert a node to the tail of the list.
      * @warning the node must not exist in any list.
      * @warning MT-UnSafe
      * @param[in] self      This object.
      * @param[in,out] n     Pointer to a new node
      */
    void (*push_back)(auto_list_t* self, auto_list_node_t* n);

    /**
     * @brief Insert a node in front of a given node.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @param[in,out] p     Pointer to a exist node
     * @param[in,out] n     Pointer to a new node
     */
    void (*insert_before)(auto_list_t* self, auto_list_node_t* p, auto_list_node_t* n);

    /**
     * @brief Insert a node right after a given node.
     * @warning the node must not exist in any list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @param[in,out] p     Pointer to a exist node
     * @param[in,out] n     Pointer to a new node
     */
    void (*insert_after)(auto_list_t* self, auto_list_node_t* p, auto_list_node_t* n);

    /**
     * @brief Delete a exist node
     * @warning The node must already in the list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @param[in,out] n     The node you want to delete
     */
    void (*erase)(auto_list_t* self, auto_list_node_t* n);

    /**
     * @brief Get the number of nodes in the list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return          The number of nodes
     */
    size_t (*size)(const auto_list_t* handler);

    /**
     * @brief Get the first node and remove it from the list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return              The first node
     */
    auto_list_node_t* (*pop_front)(auto_list_t* self);

    /**
     * @brief Get the last node and remove it from the list.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return              The last node
     */
    auto_list_node_t* (*pop_back)(auto_list_t* self);

    /**
     * @brief Get the first node.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return              The first node
     */
    auto_list_node_t* (*begin)(const auto_list_t* self);

    /**
     * @brief Get the last node.
     * @warning MT-UnSafe
     * @param[in] self      This object.
     * @return              The last node
     */
    auto_list_node_t* (*end)(const auto_list_t* self);

    /**
     * @brief Get next node.
     * @warning MT-UnSafe
     * @param[in] node      Current node.
     * @return              The next node
     */
    auto_list_node_t* (*next)(const auto_list_node_t* node);

    /**
     * @brief Get previous node.
     * @warning MT-UnSafe
     * @param[in] node      current node
     * @return              previous node
     */
    auto_list_node_t* (*prev)(const auto_list_node_t* node);

    /**
     * @brief Move all elements from \p src into the end of \p dst.
     * @warning MT-UnSafe
     * @param[in] dst   Destination list.
     * @param[in] src   Source list.
     */
    void (*migrate)(auto_list_t* dst, auto_list_t* src);
} auto_api_list_t;

/**
 * @brief Map API.
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
    void (*init)(auto_map_t* self, auto_map_cmp_fn cmp, void *arg);

    /**
     * @brief Insert the node into map.
     * @warning MT-UnSafe
     * @warning the node must not exist in any map.
     * @param[in] self      The pointer to the map
     * @param[in] node      The node
     * @return              NULL if success, otherwise return the original node.
     */
    auto_map_node_t* (*insert)(auto_map_t* self, auto_map_node_t* node);

    /**
     * @brief Replace a existing data with \p node.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @param[in] node  The node to insert.
     * @return          NULL if no existing data, otherwise return the replaced node.
     */
    auto_map_node_t* (*replace)(auto_map_t* self, auto_map_node_t* node);

    /**
     * @brief Delete the node from the map.
     * @warning The node must already in the map.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @param[in] node  The node
     */
    void (*erase)(auto_map_t* self, auto_map_node_t* node);

    /**
     * @brief Get the number of nodes in the map.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @return          The number of nodes
     */
    size_t (*size)(const auto_map_t* self);

    /**
     * @brief Finds element with specific key.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @param[in] key   The key
     * @return          An iterator point to the found node
     */
    auto_map_node_t* (*find)(const auto_map_t* self, const auto_map_node_t* key);

    /**
     * @brief Returns an iterator to the first element not less than the given key.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @param[in] key   The key
     * @return          An iterator point to the found node
     */
    auto_map_node_t* (*find_lower)(const auto_map_t* self, const auto_map_node_t* key);

    /**
     * @brief Returns an iterator to the first element greater than the given key.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @param[in] key   The key
     * @return          An iterator point to the found node
     */
    auto_map_node_t* (*find_upper)(const auto_map_t* self, const auto_map_node_t* key);

    /**
     * @brief Returns an iterator to the beginning.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @return          An iterator
     */
    auto_map_node_t* (*begin)(const auto_map_t* self);

    /**
     * @brief Returns an iterator to the end.
     * @warning MT-UnSafe
     * @param[in] self  The pointer to the map.
     * @return          An iterator
     */
    auto_map_node_t* (*end)(const auto_map_t* self);

    /**
     * @brief Get an iterator next to the given one.
     * @warning MT-UnSafe
     * @param[in] node  Current iterator
     * @return          Next iterator
     */
    auto_map_node_t* (*next)(const auto_map_node_t* node);

    /**
     * @brief Get an iterator before the given one.
     * @warning MT-UnSafe
     * @param[in] node  Current iterator
     * @return          Previous iterator
     */
    auto_map_node_t* (*prev)(const auto_map_node_t* node);
} auto_api_map_t;

/**
 * @brief Misc API.
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
 */
typedef struct auto_api_sem_s
{
    /**
     * @brief Create a new semaphore.
     * @note MT-Safe
     * @param[in] value     Initial semaphore value.
     * @return              Semaphore object.
     */
    auto_sem_t* (*create)(unsigned int value);

    /**
     * @brief Destroy semaphore.
     * @warning MT-UnSafe
     * @param[in] self  This object.
     */
    void (*destroy)(auto_sem_t* self);

    /**
     * @brief Wait for signal.
     * @note MT-Safe
     * @param[in] self  This object.
     */
    void (*wait)(auto_sem_t* self);

    /**
     * @brief Post signal.
     * @note MT-Safe
     * @param[in] self  This object.
     */
    void (*post)(auto_sem_t* self);
} auto_api_sem_t;

/**
 * @brief Misc API.
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
    auto_thread_t* (*create)(auto_thread_fn fn, void *arg);

    /**
     * @brief Wait for thread finish and release this object.
     * @warning MT-UnSafe
     * @param[in] self  This object.
     */
    void (*join)(auto_thread_t* self);

    /**
     * @brief Causes the calling thread to sleep for \p ms milliseconds.
     * @note MT-Safe
     * @param[in] ms    Milliseconds.
     */
    void (*sleep)(uint32_t ms);
} auto_api_thread_t;

typedef enum auto_coroutine_state_e
{
    /**
     * @brief The coroutine is in WAIT state and will not be scheduled.
     */
    AUTO_COROUTINE_WAIT     = 0,

    /**
     * @brief The coroutine is in BUSY state, and scheduler will process this
     *   coroutine soon.
     */
    AUTO_COROUTINE_BUSY     = 1,

    /**
     * @brief The coroutine is dead, and will be destroy soon.
     */
    AUTO_COROUTINE_DEAD     = 2,

    /**
     * @brief The coroutine contains error.
     */
    AUTO_COROUTINE_ERROR    = 4,
} auto_coroutine_state_t;

/**
 * @brief Coroutine API.
 */
typedef struct auto_api_coroutine_s
{
    /**
     * @brief Register lua coroutine \p L and let scheduler manage it's life cycle.
     *
     * A new object #auto_coroutine_t mapping to this lua coroutine is created,
     * you can use this object to do some manage operations.
     *
     * You must use this object carefully, as the life cycle is managed by
     * scheduler. To known when the object is destroyed, register schedule
     * callback by #auto_coroutine_t::hook().
     *
     * @note A lua coroutine can only register once.
     * @note This function does not affect lua stack.
     * @warning MT-UnSafe
     * @param[in] L     The coroutine created by `lua_newthread()`.
     * @return          A mapping object.
     */
    auto_coroutine_t* (*host)(struct lua_State *L);

    /**
     * @brief Find mapping coroutine object from lua coroutine \p L.
     * @warning MT-UnSafe
     * @param[in] L     The coroutine created by `lua_newthread()`.
     * @return          The mapping coroutine object, or `NULL` if not found.
     */
    auto_coroutine_t* (*find)(struct lua_State* L);

    /**
     * @brief Add schedule hook for this coroutine.
     *
     * A hook will be active every time the coroutine is scheduled.
     *
     * The hook must unregistered when coroutine finish execution or error
     * happen (That is, the #auto_coroutine_t::status is not `LUA_TNONE` or
     * `LUA_YIELD`).
     *
     * @warning MT-UnSafe
     * @note You can not call `lua_yield()` in the callback.
     * @param[in] token Hook token.
     * @param[in] fn    Schedule callback.
     * @param[in] arg   User defined data passed to \p fn.
     */
    auto_coroutine_hook_t* (*hook)(auto_coroutine_t* self, auto_coroutine_hook_fn fn, void* arg);

    /**
     * @brief Unregister schedule hook.
     * @warning MT-UnSafe
     * @param[in] self  This object.
     * @param[in] token Schedule hook return by #auto_coroutine_t::hook().
     */
    void (*unhook)(auto_coroutine_t* self, auto_coroutine_hook_t* token);

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
     * @param[in] busy  New schedule state. It only can be bit-or of #auto_coroutine_state_t.
     */
    void (*set_state)(auto_coroutine_t* self, int state);
} auto_api_coroutine_t;

/**
 * @brief Timer API.
 */
typedef struct auto_api_timer_s
{
    /**
     * @brief Create a new timer.
     * @warning MT-UnSafe
     * @return          Timer object.
     */
    auto_timer_t* (*create)(struct lua_State* L);

    /**
     * @brief Destroy timer.
     * @warning MT-UnSafe
     * @param[in] self  This object.
     */
    void (*destroy)(auto_timer_t* self);

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
    void (*start)(auto_timer_t* self, uint64_t timeout, uint64_t repeat,
                  auto_timer_fn fn, void* arg);

    /**
     * @brief Stop the timer.
     * @warning MT-UnSafe
     * @param[in] self  This object.
     */
    void (*stop)(auto_timer_t* self);
} auto_api_timer_t;

/**
 * @brief Async API.
 */
typedef struct auto_api_notify_s
{
    /**
     * @brief Create a new async object.
     * @note You must release this object before script exit.
     * @warning MT-UnSafe
     * @param[in] fn    Active callback.
     * @param[in] arg   User defined data passed to \p fn.
     * @return          Async object.
     */
    auto_notify_t* (*create)(struct lua_State* L, auto_notify_fn fn, void *arg);

    /**
     * @brief Destroy this object.
     * @warning MT-UnSafe
     * @param[in] self  This object.
     */
    void (*destroy)(auto_notify_t* self);

    /**
     * @brief Wakeup callback.
     * @note MT-Safe
     * @param[in] self  This object.
     */
    void (*send)(auto_notify_t* self);
} auto_api_notify_t;

struct auto_regex_code_s;
typedef struct auto_regex_code_s auto_regex_code_t;

/**
 * @brief Regex match callback.
 * @param[in] data      Original data to match.
 * @param[in] groups    Capture groups.
 * @param[in] group_sz  Group size.
 * @param[in] arg       User defined arguments.
 */
typedef void (*auto_regex_cb)(const char* data, size_t* groups, size_t group_sz, void* arg);

typedef struct auto_api_regex_s
{
    /**
     * @brief Compile regex \p pattern into regex bytecode.
     * @param[in] pattern   Regex pattern.
     * @param[in] size      Regex pattern size.
     * @return              Regex bytecode.
     */
    auto_regex_code_t* (*create)(const char* pattern, size_t size);

    /**
     * @brief Release regex bytecode.
     * @param[in] self      Regex bytecode.
     */
    void(*destroy)(auto_regex_code_t* self);

    /**
     * @brief Get group count.
     * @param[in] code      Regex bytecode.
     * @return              Group count.
     */
    size_t (*get_group_count)(const auto_regex_code_t* code);

    /**
     * @brief
     * @param[in] self      Compiled regular expression.
     * @param[in] data      The string to match.
     * @param[in] size      The string length in bytes.
     * @param[in] cb        Match callback. It is only called if match success.
     * @param[in] arg       User defined arguments.
     * @return              The number of groups captured.
     */
    int (*match)(const auto_regex_code_t* self, const char* data, size_t size,
        auto_regex_cb cb, void* arg);
} auto_api_regex_t;

#define AUTO_LUA_OPEQ           0
#define AUTO_LUA_OPLT           1
#define AUTO_LUA_OPLE           2

#define AUTO_LUA_TNONE          (-1)
#define AUTO_LUA_TNIL           0
#define AUTO_LUA_TBOOLEAN       1
#define AUTO_LUA_TLIGHTUSERDATA 2
#define AUTO_LUA_TNUMBER        3
#define AUTO_LUA_TSTRING        4
#define AUTO_LUA_TTABLE         5
#define AUTO_LUA_TFUNCTION      6
#define AUTO_LUA_TUSERDATA      7
#define AUTO_LUA_TTHREAD        8

#define AUTO_LUA_NOREF          (-2)
#define AUTO_LUA_REFNIL         (-1)

#define AUTO_LUA_REGISTRYINDEX  (-1001000)

/**
 * @brief Type for continuation functions.
 */
typedef int (*auto_lua_KFunction) (struct lua_State* L, int status, void* ctx);

/**
 * @brief Type for C functions.
 */
typedef int (*auto_lua_CFunction) (struct lua_State* L);

typedef struct auto_luaL_Reg {
    const char*         name;
    auto_lua_CFunction  func;
} auto_luaL_Reg;

typedef struct auto_api_lua_s
{
    /**
     * @brief Calls a function.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_callk
     * @param[in] L     Lua VM.
     * @param[in] nargs The number of arguments.
     * @param[in] nrets The number of results.
     * @param[in] ctx   Context passed to \p k.
     * @param[in] k     Continuation function.
     */
    void (*callk)(struct lua_State* L, int nargs, int nrets, void* ctx, auto_lua_KFunction k);

    /**
     * @brief Compares two Lua values.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_compare
     * @param[in] L     Lua VM.
     * @param[in] idx1  Value index 1.
     * @param[in] idx2  Value index 2.
     * @param[in] op    #AUTO_LUA_OPEQ, #AUTO_LUA_OPLT or #AUTO_LUA_OPLE.
     * @return
     */
    int (*compare)(struct lua_State* L, int idx1, int idx2, int op);

    /**
     * @brief Concatenates the \p n values at the top of the stack, pops them,
     *   and leaves the result on the top.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_concat
     * @param[in] L     Lua VM.
     * @param[in] n     The number of values.
     */
    void (*concat)(struct lua_State* L, int n);

    /**
     * @brief Pushes onto the stack the value t[k]
     * @see https://www.lua.org/manual/5.4/manual.html#lua_getfield
     * @param[in] L     Lua VM.
     * @param[in] idx   The table at the given index
     * @param[in] k     The key value.
     * @return          The type of the pushed value.
     */
    int (*getfield)(struct lua_State* L, int idx, const char* k);

    /**
     * @brief Pushes onto the stack the value of the global \p name.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_getglobal
     * @param[in] L     Lua VM.
     * @param[in] name  Global variable name.
     * @return          Returns the type of that value.
     */
    int (*getglobal)(struct lua_State* L, const char* name);

    /**
     * @brief Pushes onto the stack the value t[i].
     * @see https://www.lua.org/manual/5.4/manual.html#lua_geti
     * @param[in] L     Lua VM.
     * @param[in] idx   The value at the given index.
     * @param[in] i     The key value.
     * @return          The type of the pushed value.
     */
    int (*geti)(struct lua_State* L, int idx, int64_t i);

    /**
     * @brief Pushes onto the stack the n-th user value associated with the
     *   full userdata at the given index and returns the type of the pushed
     *   value.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_getiuservalue
     * @param[in] L     Lua VM.
     * @param[in] idx   Full userdata index.
     * @param[in] n     n-th user value
     * @return          The type of the pushed value.
     */
    int (*getiuservalue)(struct lua_State* L, int idx, int n);

    /**
     * @brief Pushes onto the stack the value t[k], where t is the value at the
     *   given index and k is the value on the top of the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_gettable
     * @param[in] L     Lua VM.
     * @param[in] idx   The table index.
     * @return          The type of the pushed value.
     */
    int (*gettable)(struct lua_State* L, int idx);

    /**
     * @brief Returns the index of the top element in the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_gettop
     * @param[in] L     Lua VM.
     * @return          The index of the top element in the stack.
     */
    int (*gettop)(struct lua_State* L);

    /**
     * @brief Moves the top element into the given valid \p index, shifting up
     *   the elements above this \p index to open space.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_insert
     * @param[in] L     Lua VM.
     * @param[in] idx   Stack index.
     */
    void (*insert)(struct lua_State* L, int idx);

    /**
     * @brief Returns 1 if the given coroutine can yield, and 0 otherwise.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_isyieldable
     * @param[in] L     Lua thread.
     * @return          Boolean.
     */
    int (*isyieldable)(struct lua_State* L);

    /**
     * @brief Creates a new empty table and pushes it onto the stack.
     * @see http://www.lua.org/manual/5.4/manual.html#lua_newtable
     * @param[in] L     Lua VM.
     */
    void (*newtable)(struct lua_State* L);

    /**
     * @brief Creates a new thread, pushes it on the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_newthread
     * @param[in] L     Lua thread.
     * @return          New lua thread.
     */
    struct lua_State* (*newthread)(struct lua_State* L);

    /**
     * @brief This function creates and pushes on the stack a new full
     *   userdata, with \p nuv associated Lua values, called user values,
     *   plus an associated block of raw memory with \p sz bytes.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_newuserdatauv
     * @param[in] L     Lua VM.
     * @param[in] sz    Memory block bytes.
     * @param[in] nuv   Number of user values.
     * @return          The address of the block of memory.
     */
    void* (*newuserdatauv)(struct lua_State* L, size_t sz, int nuv);

    /**
     * @brief Pops a key from the stack, and pushes a key–value pair from the
     *   table at the given index, the "next" pair after the given key.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_next
     * @param[in] L     Lua VM.
     * @param[in] idx   The table index.
     * @return          Boolean.
     */
    int (*next)(struct lua_State* L, int idx);

    /**
     * @brief Pops \p n elements from the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pop
     * @param[in] L     Lua VM.
     * @param[in] n     The number of values.
     */
    void (*pop)(struct lua_State* L, int n);

    /**
     * @brief Pushes a boolean value with value \p b onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushboolean
     * @param[in] L     Lua VM.
     * @param[in] b     Boolean value.
     */
    void (*pushboolean)(struct lua_State* L, int b);

    /**
     * @brief Pushes a new C closure onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushcclosure
     * @param[in] L     Lua VM.
     * @param[in] fn    Function address.
     * @param[in] n     How many upvalues this function will have.
     */
    void (*pushcclosure)(struct lua_State *L, auto_lua_CFunction fn, int n);

    /**
     * @brief Pushes a C function onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushcfunction
     * @param[in] L     Lua VM.
     * @param[in] f     Function address.
     */
    void (*pushcfunction)(struct lua_State* L, auto_lua_CFunction f);

    /**
     * @brief Pushes onto the stack a formatted string and returns a pointer to
     *   this string.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushfstring
     * @param[in] L     Lua VM.
     * @param[in] fmt   Format string.
     * @param[in] ...   Format arguments.
     * @return          A pointer of this string.
     */
    const char* (*pushfstring)(struct lua_State* L, const char *fmt, ...);

    /**
     * @brief Pushes an integer with value \p n onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushinteger
     * @param[in] L     Lua VM.
     * @param[in] n     Integer value.
     */
    void (*pushinteger)(struct lua_State* L, int64_t n);

    /**
     * @brief Pushes a light userdata onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushlightuserdata
     * @param[in] L     Lua VM.
     * @param[in] p     Pointer value.
     */
    void (*pushlightuserdata)(struct lua_State* L, void* p);

    /**
     * @brief Pushes the string pointed to by \p s with size \p len onto the
     *   stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushlstring
     * @param[in] L     Lua VM.
     * @param[in] s     String.
     * @param[in] len   String length in bytes.
     * @return          A pointer to the internal copy of the string.
     */
    const char* (*pushlstring)(struct lua_State* L, const char* s, size_t len);

    /**
     * @brief Pushes a nil value onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushnil
     * @param[in] L     Lua VM.
     */
    void (*pushnil)(struct lua_State* L);

    /**
     * @brief Pushes a float with value n onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushnumber
     * @param[in] L     Lua VM.
     * @param[in] n     Float value.
     */
    void (*pushnumber)(struct lua_State* L, double n);

    /**
     * @brief Pushes the zero-terminated string pointed to by \p s onto the
     *   stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushstring
     * @param[in] L     Lua VM.
     * @param[in] s     String.
     * @return          A pointer to the internal copy of the string.
     */
    const char* (*pushstring)(struct lua_State* L, const char* s);

    /**
     * @brief Pushes a copy of the element at the given index onto the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushvalue
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     */
    void (*pushvalue)(struct lua_State* L, int idx);

    /**
     * @brief Pushes onto the stack a formatted string and returns a pointer to
     *   this string.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_pushvfstring
     * @param[in] L     Lua VM.
     * @param[in] fmt   String format.
     * @param[in] argp  String arguments.
     * @return          A pointer to this string.
     */
    const char* (*pushvfstring)(struct lua_State* L, const char *fmt, va_list argp);

    /**
     * @brief Pushes onto the stack the value t[n].
     * @see https://www.lua.org/manual/5.4/manual.html#lua_rawgeti
     * @param[in] L     Lua VM.
     * @param idx
     * @param n
     * @return
     */
    int (*rawgeti)(struct lua_State* L, int idx, int64_t n);

    /**
     * @brief Removes the element at the given valid index, shifting down the
     *   elements above this index to fill the gap.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_remove
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     */
    void (*remove)(struct lua_State* L, int idx);

    /**
     * @brief Moves the top element into the given valid index without shifting
     *   any element,and then pops the top element.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_replace
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     */
    void (*replace)(struct lua_State* L, int idx);

    /**
     * @brief Rotates the stack elements between the valid index \p idx and the
     *   top of the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_rotate
     * @param[in] L     Lua VM.
     * @param[in] idx   Valid stack index.
     * @param[in] n     Positions in the direction of the top.
     */
    void (*rotate)(struct lua_State* L, int idx, int n);

    /**
     * @brief Does the equivalent to t[k] = v.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_setfield
     * @param[in] L     Lua VM.
     * @param[in] idx   Table index.
     * @param[in] k     Key value.
     */
    void (*setfield)(struct lua_State* L, int idx, const char* k);

    /**
     * @brief Pops a value from the stack and sets it as the new value of
     *   global \p name.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_setglobal
     * @param[in] L     Lua VM.
     * @param[in] name  Global name.
     */
    void (*setglobal)(struct lua_State* L, const char* name);

    /**
     * @brief Does the equivalent to t[n] = v.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_seti
     * @param[in] L     Lua VM.
     * @param[in] idx   Table index.
     * @param[in] n     Key value.
     */
    void (*seti)(struct lua_State* L, int idx, int64_t n);

    /**
     * @brief Pops a value from the stack and sets it as the new n-th user
     *   value associated to the full userdata at the given index.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_setiuservalue
     * @param[in] L     Lua VM.
     * @param[in] idx   User data index.
     * @param[in] n     n-th user value.
     * @return          0 if the userdata does not have that value.
     */
    int (*setiuservalue)(struct lua_State* L, int idx, int n);

    /**
     * @brief Pops a table or nil from the stack and sets that value as the new
     *   metatable for the value at the given index.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_setmetatable
     * @param[in] L     Lua VM.
     * @param[in] idx   Stack index.
     * @return          Always 1
     */
    int (*setmetatable)(struct lua_State* L, int idx);

    /**
     * @brief Does the equivalent to t[k] = v.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_settable
     * @param[in] L     Lua VM.
     * @param[in] idx   Table index.
     */
    void (*settable)(struct lua_State* L, int idx);

    /**
     * @brief Accepts any index, or 0, and sets the stack top to this index.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_settop
     * @param[in] L     Lua VM.
     * @param[in] idx   Stack index.
     */
    void (*settop)(struct lua_State* L, int idx);

    /**
     * @brief Converts the Lua value at the given index to a C boolean value.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_toboolean
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @return          Boolean.
     */
    int (*toboolean)(struct lua_State* L, int idx);

    /**
     * @brief Converts a value at the given index to a C function.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_tocfunction
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @return          C function address.
     */
    auto_lua_CFunction (*tocfunction)(struct lua_State* L, int idx);

    /**
     * @brief Converts the Lua value at the given index to the signed integer.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_tointeger
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @return          Signed integer.
     */
    int64_t (*tointeger)(struct lua_State* L, int idx);

    /**
     * @brief Converts the Lua value at the given index to a C string.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_tolstring
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @param[out] len  String length in bytes.
     * @return          C string pointer.
     */
    const char* (*tolstring)(struct lua_State* L, int idx, size_t *len);

    /**
     * @brief Converts the Lua value at the given index to the float number.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_tonumber
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @return          Float number.
     */
    double (*tonumber)(struct lua_State* L, int idx);

    /**
     * @brief Converts the Lua value at the given index to a C string.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_tostring
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @return          C string.
     */
    const char* (*tostring)(struct lua_State* L, int idx);

    /**
     * @brief Converts the Lua value at the given index to a userdata address.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_touserdata
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @return          If the value at the given index is a full userdata,
     *   returns its memory-block address. If the value is a light userdata,
     *   returns its value (a pointer). Otherwise, returns NULL.
     */
    void* (*touserdata)(struct lua_State* L, int idx);

    /**
     * @brief Returns the type of the value in the given valid index, or
     *   #AUTO_LUA_TNONE for a non-valid but acceptable index.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_type
     * @param[in] L     Lua VM.
     * @param[in] idx   Value index.
     * @return          One of #AUTO_LUA_TNONE, #AUTO_LUA_TNIL,
     *   #AUTO_LUA_TBOOLEAN, #AUTO_LUA_TLIGHTUSERDATA, #AUTO_LUA_TNUMBER,
     *   #AUTO_LUA_TSTRING, #AUTO_LUA_TTABLE, #AUTO_LUA_TFUNCTION,
     *   #AUTO_LUA_TUSERDATA, #AUTO_LUA_TTHREAD.
     */
    int (*type)(struct lua_State* L, int idx);

    /**
     * @brief Yields a coroutine.
     * @see https://www.lua.org/manual/5.4/manual.html#lua_yieldk
     * @param[in] L     Lua VM.
     * @param[in] nrets The number of results.
     * @param[in] ctx   Context passed to \p k.
     * @param[in] k     Continuation function.
     * @return          This function does not return.
     */
    int (*yieldk)(struct lua_State *L, int nrets, void* ctx, auto_lua_KFunction k);

    /**
     * @brief Calls a function.
     *
     * Usage:
     * ```c
     * return a_callk(L, nargs, nrets, ctx, k);
     * ```
     *
     * It is equal to:
     * ```c
     * lua_callk(L, nargs, nrets, ctx, k);
     * return k(L, LUA_OK, ctx);
     * ```
     *
     * @param[in] L     Lua VM.
     * @param[in] nargs The number of arguments.
     * @param[in] nrets The number of results.
     * @param[in] ctx   Context passed to \p k.
     * @param[in] k     Continuation function.
     * @return          The same as \p k. This function might not return, and
     *   should be the last call in function.
     */
    int (*A_callk)(struct lua_State* L, int nargs, int nrets, void* ctx, auto_lua_KFunction k);

    /**
     * @brief Checks whether the function argument arg is an integer (or can be
     *   converted to an integer) and returns this integer.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_checkinteger
     * @param[in] L     Lua VM.
     * @param[in] arg   Argument index.
     * @return          Integer.
     */
    int64_t (*L_checkinteger)(struct lua_State* L, int arg);

    /**
     * @brief Checks whether the function argument \p arg is a string and
     *   returns this string.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_checklstring
     * @param[in] L     Lua VM.
     * @param[in] arg   Argument index.
     * @param[out] l    String length in bytes.
     * @return          String pointer.
     */
    const char* (*L_checklstring)(struct lua_State* L, int arg, size_t *l);

    /**
     * @brief Checks whether the function argument arg is a number and returns
     *   this number.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_checknumber
     * @param[in] L     Lua VM.
     * @param[in] arg   Argument index.
     * @return          Number
     */
    double (*L_checknumber)(struct lua_State* L, int arg);

    /**
     * @brief Checks whether the function argument \p arg is a string and
     *   returns this string.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_checkstring
     * @param[in] L     Lua VM.
     * @param[in] arg   Argument index.
     * @return          String.
     */
    const char* (*L_checkstring)(struct lua_State* L, int arg);

    /**
     * @brief Checks whether the function argument \p arg has type \p t.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_checktype
     * @see #auto_api_lua_t::type()
     * @param[in] L     Lua VM.
     * @param[in] arg   Argument index.
     * @param[in] t     Type.
     */
    void (*L_checktype)(struct lua_State* L, int arg, int t);

    /**
     * @brief Checks whether the function argument \p arg is a userdata of the
     *   type \p tname.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_checkudata
     * @param[in] L     Lua VM.
     * @param[in] arg   Argument index.
     * @param[in] tname Type name.
     */
    void (*L_checkudata)(struct lua_State* L, int arg, const char *tname);

    /**
     * @brief Raises an error.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_error
     * @param[in] L     Lua VM.
     * @param[in] fmt   String format.
     * @param[in] ...   Format arguments.
     * @return          This function never returns.
     */
    int (*L_error)(struct lua_State* L, const char *fmt, ...);

    /**
     * @brief Creates a copy of string \p s, replacing any occurrence of the
     *   string \p p with the string \p r. Pushes the resulting string on the
     *   stack.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_gsub
     * @param[in] L     Lua VM.
     * @param[in] s     Original string.
     * @param[in] p     String to be replaced.
     * @param[in] r     The replace string.
     * @return          A string pointer of result.
     */
    const char* (*L_gsub)(struct lua_State* L, const char *s, const char *p, const char *r);

    /**
     * @brief Returns the "length" of the value at the given index as a number.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_len
     * @param[in] L     Lua VM.
     * @param[in] idx   Stack index.
     * @return          Length.
     */
    int64_t (*L_len)(struct lua_State* L, int idx);

    /**
     * @brief Creates a new table and registers there the functions in the
     *   list \p l.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_newlib
     * @param[in] L     Lua VM.
     * @param[in] l     Function list.
     */
    void (*L_newlib)(struct lua_State* L, const auto_luaL_Reg l[]);

    /**
     * @brief If the registry already has the key \p tname, returns 0.
     *   Otherwise, creates a new table to be used as a metatable for userdata,
     *   adds to this new table the pair __name = \p tname, adds to the
     *   registry the pair [tname] = new table, and returns 1.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_newmetatable
     * @param[in] L     Lua VM.
     * @param[in] tname Type name.
     * @return          Boolean.
     */
    int (*L_newmetatable)(struct lua_State* L, const char* tname);

    /**
     * @brief Creates and returns a reference.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_ref
     * @param[in] L     Lua VM.
     * @param[in] t     Table index. Use #AUTO_LUA_REGISTRYINDEX for pseudo-index.
     * @return          Reference.
     */
    int (*L_ref)(struct lua_State* L, int t);

    /**
     * @brief Registers all functions in the array \p l into the table on the
     *   top of the stack.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_setfuncs
     * @param[in] L     Lua VM.
     * @param[in] l     Function array.
     * @param[in] nup   Number of upvalues.
     */
    void (*L_setfuncs)(struct lua_State* L, const auto_luaL_Reg* l, int nup);

    /**
     * @brief Returns the name of the type encoded by the value \p tp.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_typename
     * @param[in] L     Lua VM.
     * @param[in] tp    Return value from #auto_api_lua_t::type().
     * @return          Name string.
     */
    const char* (*L_typename)(struct lua_State* L, int tp);

    /**
     * @brief Releases the reference \p ref from the table at index \p t.
     * @see https://www.lua.org/manual/5.4/manual.html#luaL_unref
     * @param[in] L     Lua VM.
     * @param[in] t     Table index.
     * @param[in] ref   Reference.
     */
    void (*L_unref)(struct lua_State* L, int t, int ref);
} auto_api_lua_t;

struct auto_async_s;
typedef struct auto_async_s auto_async_t;

typedef void (*auto_async_cb)(struct lua_State* L, void* arg);

typedef struct auto_api_async_s
{
    /**
     * @brief Create async handle.
     * @param[in] L     The lua thread that code running on.
     * @return          Async handle.
     */
    auto_async_t* (*create)(struct lua_State* L);

    /**
     * @brief Destroy async handle.
     * @param[in] self  Async handle.
     */
    void (*destroy)(auto_async_t* self);

    /**
     * @brief Call \p cb in lua thread.
     * @param[in] self  Async handle.
     * @param[in] cb    Callback.
     * @param[in] arg   User defined argument pass to \p cb.
     * @return          Boolean.
     */
    int (*call_in_lua)(auto_async_t* self, auto_async_cb cb, void* arg);

    /**
     * @brief Cancel all pending task.
     * @param[in] self  Async handle.
     */
    void (*cancel_all)(auto_async_t* self);
} auto_api_async_t;

/**
 * @brief API.
 */
typedef struct auto_api_s
{
    const auto_api_lua_t*       lua;
    const auto_api_memory_t*    memory;
    const auto_api_list_t*      list;
    const auto_api_map_t*       map;
    const auto_api_sem_t*       sem;
    const auto_api_thread_t*    thread;
    const auto_api_timer_t*     timer;
    const auto_api_notify_t*    notify;
    const auto_api_coroutine_t* coroutine;
    const auto_api_misc_t*      misc;
    const auto_api_regex_t*     regex;
    const auto_api_async_t*     async;
} auto_api_t;

/**
 * @brief Get Exposed API.
 * @return  Exposed API.
 */
AUTO_PUBLIC const auto_api_t* auto_api(void);

#ifdef __cplusplus
}
#endif

#endif
