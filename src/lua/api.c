#include <stdlib.h>
#include <string.h>
#include "runtime.h"
#include "lua/coroutine.h"
#include "lua/int64.h"
#include "lua/process.h"
#include "lua/screenshot.h"
#include "lua/sleep.h"
#include "utils.h"

/******************************************************************************
* Expose lua api and c api to lua vm
******************************************************************************/

/**
 * @brief Lua API list.
 */
#define AUTO_LUA_API_MAP(xx) \
    xx("coroutine",         atd_lua_coroutine)      \
    xx("process",           atd_lua_process)        \
    xx("screenshot",        atd_lua_screenshot)     \
    xx("sleep",             atd_lua_sleep)

#define EXPAND_MAP_AS_LUA_FUNCTION(name, func) \
    { name, func },

static const luaL_Reg s_funcs[] = {
    AUTO_LUA_API_MAP(EXPAND_MAP_AS_LUA_FUNCTION)
    { NULL, NULL }
};

#undef EXPAND_MAP_AS_LUA_FUNCTION

static int _auto_get_c_api(lua_State *L)
{
    unsigned major = lua_tointeger(L, 1);
    unsigned minor = lua_tointeger(L, 2);
    unsigned patch = lua_tointeger(L, 3);
    lua_pushlightuserdata(L, (void*)auto_get_api(major, minor, patch));
    return 1;
}

void auto_init_libs(lua_State *L)
{
    /* Create lua api table */
    luaL_newlib(L, s_funcs);

    /* Register C api */
    lua_pushcfunction(L, _auto_get_c_api);
    lua_setfield(L, -2, "get_c_api");

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

static atd_sync_t* api_async_create(atd_async_fn fn, void* arg)
{
    atd_sync_t* impl = malloc(sizeof(atd_sync_t));
    impl->fn = fn;
    impl->arg = arg;
    uv_async_init(&g_rt->loop, &impl->async, _async_on_active);

    return impl;
}

/******************************************************************************
* C API: .timer
******************************************************************************/

struct atd_timer_s
{
    uv_timer_t      timer;
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

static atd_timer_t* api_timer_create(void)
{
    atd_timer_t* impl = malloc(sizeof(atd_timer_t));
    uv_timer_init(&g_rt->loop, &impl->timer);
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

const auto_api_t api = {
    {
        ev_list_init,                       /* .list.init */
        ev_list_push_front,                 /* .list.push_front */
        ev_list_push_back,                  /* .list.push_back */
        ev_list_insert_before,              /* .list.insert_before */
        ev_list_insert_after,               /* .list.insert_after */
        ev_list_erase,                      /* .list.erase */
        ev_list_size,                       /* .list.size */
        ev_list_pop_front,                  /* .list.pop_front */
        ev_list_pop_back,                   /* .list.pop_back */
        ev_list_begin,                      /* .list.begin */
        ev_list_end,                        /* .list.end */
        ev_list_next,                       /* .list.next */
        ev_list_prev,                       /* .list.prev */
        ev_list_migrate,                    /* .list.migrate */
    },
    {
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
    },
    {
        api_sem_create,                     /* .sem.create */
        api_sem_destroy,                    /* .sem.destroy */
        api_sem_wait,                       /* .sem.wait */
        api_sem_post,                       /* .sem.post */
    },
    {
        api_thread_create,                  /* .thread.create */
        api_thread_join,                    /* .thread.join */
        uv_sleep,                           /* .thread.sleep */
    },
    {
        api_async_create,                   /* .async.create */
        api_async_destroy,                  /* .async.destroy */
        api_async_send,                     /* .async.send */
    },
    {
        api_timer_create,                   /* .timer.create */
        api_timer_destroy,                  /* .timer.destroy */
        api_timer_start,                    /* .timer.start */
        api_timer_stop,                     /* .timer.stop */
    },
    {
        api_process_create,                 /* .process.create */
        api_process_kill,                   /* .process.kill */
        api_process_send_to_stdin,          /* .process.send_to_stdin */
    },
    {
        api_coroutine_host,                 /* .coroutine.host */
        api_coroutine_find,                 /* .coroutine.find */
        api_coroutine_hook,                 /* .coroutine.hook */
        api_coroutine_unhook,               /* .coroutine.unhook */
        api_coroutine_set_state,            /* .coroutine.set_state */
    },
    {
        api_int64_push_value,               /* .int64.push_value */
        api_int64_get_value,                /* .int64.get_value */
    },
    {
        uv_hrtime,                          /* .misc.hrtime */
        api_search,                         /* .misc.search */
    },
};

const auto_api_t* auto_get_api(unsigned major, unsigned minor, unsigned patch)
{
    if (AUTO_VERSION_MAJOR == major && AUTO_VERSION_MINOR == minor && AUTO_VERSION_PATCH == patch)
    {
        return &api;
    }

    return NULL;
}
