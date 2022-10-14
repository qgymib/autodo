#define AUTO_API_EXPORT
#include <stdlib.h>
#include <string.h>
#include "runtime.h"
#include "api.h"
#include "lua/coroutine.h"
#include "lua/download.h"
#include "lua/int64.h"
#include "lua/process.h"
#include "lua/screenshot.h"
#include "lua/sleep.h"
#include "lua/uname.h"
#include "utils.h"

/******************************************************************************
* Expose lua api and c api to lua vm
******************************************************************************/

/**
 * @brief Lua API list.
 */
#define AUTO_LUA_API_MAP(xx) \
    xx("coroutine",         auto_new_coroutine)     \
    xx("download",          auto_lua_download)      \
    xx("process",           atd_lua_process)        \
    xx("screenshot",        atd_lua_screenshot)     \
    xx("sleep",             atd_lua_sleep)          \
    xx("uname",             auto_lua_uname)

#define EXPAND_MAP_AS_LUA_FUNCTION(name, func) \
    { name, func },

static const luaL_Reg s_funcs[] = {
    AUTO_LUA_API_MAP(EXPAND_MAP_AS_LUA_FUNCTION)
    { NULL, NULL }
};

#undef EXPAND_MAP_AS_LUA_FUNCTION

void auto_init_libs(lua_State *L)
{
    /* Create lua api table */
    luaL_newlib(L, s_funcs);

    static struct { const char* name; void* value; } s_api_list[] = {
        { "c_api_memory",       (void*)&api_memory, },
        { "c_api_list",         (void*)&api_list, },
        { "c_api_map",          (void*)&api_map, },
        { "c_api_misc",         (void*)&api_misc, },
        { "c_api_sem",          (void*)&api_sem, },
        { "c_api_thread",       (void*)&api_thread, },
        { "c_api_coroutine",    (void*)&api_coroutine, },
        { "c_api_timer",        (void*)&api_timer, },
        { "c_api_async",        (void*)&api_async, },
        { "c_api_int64",        (void*)&api_int64, },
    };

    /* Register C API */
    size_t i;
    for (i = 0; i < ARRAY_SIZE(s_api_list); i++)
    {
        lua_pushlightuserdata(L, s_api_list[i].value);
        lua_setfield(L, -2, s_api_list[i].name);
    }

    /* Set as global variable */
    lua_setglobal(L, "auto");
}

/******************************************************************************
* C API: .sem
******************************************************************************/
struct atd_sem_s
{
    uv_sem_t    sem;
};

static void api_sem_destroy(atd_sem_t* self)
{
    uv_sem_destroy(&self->sem);
    free(self);
}

static void api_sem_wait(atd_sem_t* self)
{
    uv_sem_wait(&self->sem);
}

static void api_sem_post(atd_sem_t* self)
{
    uv_sem_post(&self->sem);
}

static atd_sem_t* api_sem_create(unsigned int value)
{
    atd_sem_t* impl = malloc(sizeof(atd_sem_t));
    uv_sem_init(&impl->sem, value);
    return impl;
}

/******************************************************************************
* C API: .thread
******************************************************************************/
struct atd_thread_s
{
    uv_thread_t     thread;
};

static void api_thread_join(atd_thread_t* self)
{
    uv_thread_join(&self->thread);
    free(self);
}

static atd_thread_t* api_thread_create(atd_thread_fn fn, void* arg)
{
    atd_thread_t* impl = malloc(sizeof(atd_thread_t));
    if (uv_thread_create(&impl->thread, fn, arg) != 0)
    {
        free(impl);
        return NULL;
    }
    return impl;
}

/******************************************************************************
* C API: .async
******************************************************************************/

struct atd_sync_s
{
    uv_async_t      async;
    atd_runtime_t*  rt;
    atd_async_fn    fn;
    void*           arg;
};

static void _async_on_close(uv_handle_t* handle)
{
    atd_sync_t* impl = container_of((uv_async_t*)handle, atd_sync_t, async);
    free(impl);
}

static void _async_on_active(uv_async_t* handle)
{
    atd_sync_t* impl = container_of(handle, atd_sync_t, async);
    impl->fn(impl->arg);
}

static void api_async_destroy(atd_sync_t* self)
{
    uv_close((uv_handle_t*)&self->async, _async_on_close);
}

static void api_async_send(atd_sync_t* self)
{
    uv_async_send(&self->async);
}

static atd_sync_t* api_async_create(lua_State* L, atd_async_fn fn, void* arg)
{
    atd_runtime_t* rt = auto_get_runtime(L);

    atd_sync_t* impl = malloc(sizeof(atd_sync_t));

    impl->rt = rt;
    impl->fn = fn;
    impl->arg = arg;
    uv_async_init(&rt->loop, &impl->async, _async_on_active);

    return impl;
}

/******************************************************************************
* C API: .timer
******************************************************************************/

struct atd_timer_s
{
    uv_timer_t      timer;
    atd_runtime_t*  rt;
    atd_timer_fn    fn;
    void*           arg;
} ;

static void _on_timer_close(uv_handle_t* handle)
{
    atd_timer_t* impl = container_of((uv_timer_t*)handle, atd_timer_t, timer);
    free(impl);
}

static void _timer_on_active(uv_timer_t* handle)
{
    atd_timer_t* impl = container_of(handle, atd_timer_t, timer);
    impl->fn(impl->arg);
}

static void api_timer_destroy(atd_timer_t* self)
{
    uv_close((uv_handle_t*)&self->timer, _on_timer_close);
}

static void api_timer_start(atd_timer_t* self, uint64_t timeout,
    uint64_t repeat, atd_timer_fn fn, void* arg)
{
    self->fn = fn;
    self->arg = arg;
    uv_timer_start(&self->timer, _timer_on_active, timeout, repeat);
}

static void api_timer_stop(atd_timer_t* self)
{
    uv_timer_stop(&self->timer);
}

static atd_timer_t* api_timer_create(lua_State* L)
{
    atd_runtime_t* rt = auto_get_runtime(L);

    atd_timer_t* impl = malloc(sizeof(atd_timer_t));
    impl->rt = rt;
    uv_timer_init(&rt->loop, &impl->timer);

    return impl;
}

/******************************************************************************
* C API: .misc
******************************************************************************/

static ssize_t api_search(const void* data, size_t size, const void* key, size_t len)
{
    int32_t* fsm = malloc(sizeof(int) * len);
    ssize_t ret = aeda_find(data, size, key, len, fsm, len);
    free(fsm);
    return ret;
}

const auto_api_memory_t api_memory = {
    malloc,                             /* .mem.malloc */
    free,                               /* .mem.free */
    calloc,                             /* .mem.calloc */
    realloc,                            /* .mem.realloc */
};

const auto_api_list_t api_list = {
    ev_list_init,                       /* .init */
    ev_list_push_front,                 /* .push_front */
    ev_list_push_back,                  /* .push_back */
    ev_list_insert_before,              /* .insert_before */
    ev_list_insert_after,               /* .insert_after */
    ev_list_erase,                      /* .erase */
    ev_list_size,                       /* .size */
    ev_list_pop_front,                  /* .pop_front */
    ev_list_pop_back,                   /* .pop_back */
    ev_list_begin,                      /* .begin */
    ev_list_end,                        /* .end */
    ev_list_next,                       /* .next */
    ev_list_prev,                       /* .prev */
    ev_list_migrate,                    /* .migrate */
};

const auto_api_map_t api_map = {
    ev_map_init,                        /* .map.init */
    ev_map_insert,                      /* .map.insert */
    ev_map_replace,                     /* .map.replace */
    ev_map_erase,                       /* .map.erase */
    ev_map_size,                        /* .map.size */
    ev_map_find,                        /* .map.find */
    ev_map_find_lower,                  /* .map.find_lower */
    ev_map_find_upper,                  /* .map.find_upper */
    ev_map_begin,                       /* .map.begin */
    ev_map_end,                         /* .map.end */
    ev_map_next,                        /* .map.next */
    ev_map_prev,                        /* .map.prev */
};

const auto_api_misc_t api_misc = {
    uv_hrtime,                          /* .misc.hrtime */
    api_search,                         /* .misc.search */
};

const auto_api_sem_t api_sem = {
    api_sem_create,                     /* .sem.create */
    api_sem_destroy,                    /* .sem.destroy */
    api_sem_wait,                       /* .sem.wait */
    api_sem_post,                       /* .sem.post */
};

const auto_api_thread_t api_thread = {
    api_thread_create,                  /* .thread.create */
    api_thread_join,                    /* .thread.join */
    uv_sleep,                           /* .thread.sleep */
};

const auto_api_timer_t api_timer = {
    api_timer_create,                   /* .timer.create */
    api_timer_destroy,                  /* .timer.destroy */
    api_timer_start,                    /* .timer.start */
    api_timer_stop,                     /* .timer.stop */
};

const auto_api_async_t api_async = {
    api_async_create,                   /* .async.create */
    api_async_destroy,                  /* .async.destroy */
    api_async_send,                     /* .async.send */
};
