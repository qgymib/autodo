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

#define EXPAND_MAP_AS_PROXY_FUNCTION(name, func)    \
    static int func##_##proxy(lua_State *L) {\
        AUTO_CHECK_TERM();\
        return func(L);\
    }

AUTO_LUA_API_MAP(EXPAND_MAP_AS_PROXY_FUNCTION)

#undef EXPAND_MAP_AS_PROXY_FUNCTION

#define EXPAND_MAP_AS_LUA_FUNCTION(name, func) \
    { name, func##_##proxy },

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
* C API: .process
******************************************************************************/

struct atd_process_s
{
    uv_process_t            process;

    atd_process_stdio_fn    stdout_fn;
    atd_process_stdio_fn    stderr_fn;
    void*                   arg;

    int                     spawn_ret;
    int64_t                 exit_status;
    int                     term_signal;

    uv_pipe_t               pip_stdin;
    uv_pipe_t               pip_stdout;
    uv_pipe_t               pip_stderr;

    int                     flag_process_exit;
    int                     flag_stdin_exit;
    int                     flag_stdout_exit;
    int                     flag_stderr_exit;
};

typedef struct atd_process_write
{
    uv_write_t              req;
    atd_process_t*          impl;

    void*                   data;
    size_t                  size;

    atd_process_stdio_fn    cb;
    void*                   arg;
} atd_process_write_t;

static void _process_on_close(atd_process_t* impl)
{
    if (!impl->flag_process_exit || !impl->flag_stdin_exit || !impl->flag_stdout_exit || !impl->flag_stderr_exit)
    {
        return;
    }

    free(impl);
}

static void _process_on_process_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_process_t*)handle, atd_process_t, process);
    impl->flag_process_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stdin_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_pipe_t*)handle, atd_process_t, pip_stdin);
    impl->flag_stdin_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stdout_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_pipe_t*)handle, atd_process_t, pip_stdout);
    impl->flag_stdout_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stderr_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_pipe_t*)handle, atd_process_t, pip_stderr);
    impl->flag_stderr_exit = 1;
    _process_on_close(impl);
}

static void api_process_kill(atd_process_t* self, int signum)
{
    if (self->spawn_ret == 0)
    {
        uv_process_kill(&self->process, signum);
    }

    uv_close((uv_handle_t*)&self->process, _process_on_process_close);
    uv_close((uv_handle_t*)&self->pip_stdin, _process_on_stdin_close);

    if (!self->flag_stdout_exit)
    {
        uv_close((uv_handle_t*)&self->pip_stdout, _process_on_stdout_close);
    }
    if (!self->flag_stderr_exit)
    {
        uv_close((uv_handle_t*)&self->pip_stderr, _process_on_stderr_close);
    }
}

static void _process_write_done(uv_write_t *req, int status)
{
    atd_process_write_t* p_req = container_of(req, atd_process_write_t, req);

    p_req->cb(p_req->impl, p_req->data, p_req->size, status, p_req->arg);
    free(p_req);
}

static int api_process_send_to_stdin(atd_process_t* self, void* data,
    size_t size, atd_process_stdio_fn cb, void* arg)
{
    atd_process_write_t* p_req = malloc(sizeof(atd_process_write_t));
    p_req->impl = self;
    p_req->cb = cb;
    p_req->arg = arg;
    p_req->size = size;
    p_req->data = data;

    uv_buf_t buf = uv_buf_init(p_req->data, p_req->size);
    return uv_write(&p_req->req, (uv_stream_t*)&self->pip_stdin, &buf, 1,
        _process_write_done);
}

static void _process_on_exit(uv_process_t* process, int64_t exit_status, int term_signal)
{
    atd_process_t* impl = container_of(process, atd_process_t, process);
    impl->exit_status = exit_status;
    impl->term_signal = term_signal;
}

static void _process_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void)handle;
    buf->len = suggested_size;
    buf->base = malloc(suggested_size);
}

static void _process_on_stdout(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    atd_process_t* impl = container_of((uv_pipe_t*)stream, atd_process_t, pip_stdout);

    /* Stop read if error */
    if (nread < 0)
    {
        uv_read_stop(stream);
    }

    if (buf->base != NULL)
    {
        impl->stdout_fn(impl, buf->base, nread, nread, impl->arg);
        free(buf->base);
    }
}

static void _process_on_stderr(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    atd_process_t* impl = container_of((uv_pipe_t*)stream, atd_process_t, pip_stderr);

    /* Stop read if error */
    if (nread < 0)
    {
        uv_read_stop(stream);
    }

    if (buf->base != NULL)
    {
        impl->stderr_fn(impl, buf->base, nread, nread, impl->arg);
        free(buf->base);
    }
}

static atd_process_t* api_process_create(atd_process_cfg_t* cfg)
{
    atd_process_t* impl = malloc(sizeof(atd_process_t));
    memset(impl, 0, sizeof(*impl));

    impl->stdout_fn = cfg->stdout_fn;
    impl->stderr_fn = cfg->stderr_fn;
    impl->arg = cfg->arg;

    uv_stdio_container_t stdios[3];
    memset(stdios, 0, sizeof(stdios));

    uv_pipe_init(&g_rt->loop, &impl->pip_stdin, 0);
    stdios[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
    stdios[0].data.stream = (uv_stream_t*)&impl->pip_stdin;

    if (impl->stdout_fn != NULL)
    {
        uv_pipe_init(&g_rt->loop, &impl->pip_stdout, 0);
        stdios[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
        stdios[1].data.stream = (uv_stream_t*)&impl->pip_stdout;
    }
    else
    {
        impl->flag_stdout_exit = 1;
    }

    if (impl->stderr_fn != NULL)
    {
        uv_pipe_init(&g_rt->loop, &impl->pip_stderr, 0);
        stdios[2].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
        stdios[2].data.stream = (uv_stream_t*)&impl->pip_stderr;
    }
    else
    {
        impl->flag_stderr_exit = 1;
    }

    uv_process_options_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.exit_cb = _process_on_exit;
    opt.file = cfg->path;
    opt.args = cfg->args;
    opt.env = cfg->envs;
    opt.cwd = cfg->cwd;
    opt.stdio = stdios;
    opt.stdio_count = 3;

    impl->spawn_ret = uv_spawn(&g_rt->loop, &impl->process, &opt);
    if (impl->spawn_ret != 0)
    {
        api.process.kill(impl, SIGKILL);
        return NULL;
    }

    uv_read_start((uv_stream_t*)&impl->pip_stdout, _process_alloc_cb, _process_on_stdout);
    uv_read_start((uv_stream_t*)&impl->pip_stderr, _process_alloc_cb, _process_on_stderr);

    return impl;
}

/******************************************************************************
* C API: .coroutine
******************************************************************************/

static void api_coroutine_set_schedule_state(struct atd_coroutine* self, int state)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    /* In busy_queue */
    if (impl->base.status == LUA_TNONE)
    {
        if (state == LUA_TNONE)
        {/* Do nothing if new state is also BUSY */
            return;
        }

        ev_list_erase(&g_rt->schedule.busy_queue, &impl->q_node);
        impl->base.status = state;
        ev_list_push_back(&g_rt->schedule.wait_queue, &impl->q_node);
        return;
    }

    /* We are in wait_queue, cannot operate on dead coroutine */
    if (impl->base.status != LUA_YIELD)
    {
        abort();
    }

    /* move to busy_queue */
    if (state == LUA_TNONE)
    {
        ev_list_erase(&g_rt->schedule.wait_queue, &impl->q_node);
        impl->base.status = state;
        ev_list_push_back(&g_rt->schedule.busy_queue, &impl->q_node);
    }

    /* thr is dead, keep in wait_queue */
}

static atd_coroutine_hook_t* api_coroutine_hook(struct atd_coroutine* self, atd_coroutine_hook_fn fn, void* arg)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    atd_coroutine_hook_t* token = malloc(sizeof(atd_coroutine_hook_t));
    token->fn = fn;
    token->arg = arg;
    token->impl = impl;
    ev_list_push_back(&impl->hook.queue, &token->node);

    return token;
}

static void api_coroutine_unhook(struct atd_coroutine* self, atd_coroutine_hook_t* token)
{
    atd_coroutine_impl_t* impl = container_of(self, atd_coroutine_impl_t, base);

    if (impl->hook.it != NULL)
    {
        atd_coroutine_hook_t* next_hook = container_of(impl->hook.it, atd_coroutine_hook_t, node);

        /* If this is next hook, move iterator to next node. */
        if (next_hook == token)
        {
            impl->hook.it = ev_list_next(impl->hook.it);
        }
    }

    ev_list_erase(&impl->hook.queue, &token->node);
    token->impl = NULL;

    free(token);
}

static atd_coroutine_t* api_coroutine_host(lua_State* L)
{
    atd_coroutine_impl_t* thr = malloc(sizeof(atd_coroutine_impl_t));

    memset(thr, 0, sizeof(*thr));
    thr->base.L = L;
    thr->base.status = LUA_TNONE;
    ev_list_init(&thr->hook.queue);

    /* Save to schedule table to check duplicate */
    if (ev_map_insert(&g_rt->schedule.all_table, &thr->t_node) != NULL)
    {
        free(thr);
        return NULL;
    }

    /* Get reference */
    lua_pushthread(L);
    thr->data.ref_key = luaL_ref(L, LUA_REGISTRYINDEX);

    /* Save to busy_queue */
    ev_list_push_back(&g_rt->schedule.busy_queue, &thr->q_node);

    return &thr->base;
}

static atd_coroutine_t* api_coroutine_find(lua_State* L)
{
    atd_coroutine_impl_t tmp;
    tmp.base.L = L;

    atd_map_node_t* it = ev_map_find(&g_rt->schedule.all_table, &tmp.t_node);
    if (it == NULL)
    {
        return NULL;
    }

    atd_coroutine_impl_t* impl = container_of(it, atd_coroutine_impl_t, t_node);
    return &impl->base;
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
        api_coroutine_set_schedule_state,   /* .coroutine.set_schedule_state */
    },
    {
        api_int64_push_value,               /* .int64.push_value */
        api_int64_get_value,    /* .int64.get_value */
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
